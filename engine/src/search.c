// 本探索
//
// - 反復深化 + PVS (null window で調べて、良ければ全幅で再探索)
// - 置換表 (Zobrist ハッシュ、2 エントリのバケット、深さ優先 + 世代で置き換え)
// - 強制手: 相手に五の点があれば止める手だけ (深さを減らさない = 四への延長)。
//   相手に達四の点 (活三など) があれば、防ぎ手と自分の四だけ
// - 手の並べ替え: 置換表の手 → パターン点 (攻め + 守り) + killer + history
// - 遅い手の深さ削減 (LMR) と浅い所での手数打ち切り (戦術的でない手のみ)
#include "search.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "eval.h"
#include "vcf.h"

// ---------------------------------------------------------------- 置換表

// 複数スレッドで共有する。ロックは使わず、key には「局面のハッシュ ^ data」を入れて、
// 書き込みが混ざったエントリは key が合わなくなって捨てられるようにする
typedef struct {
    uint64_t key;
    uint64_t data;  // value 16 | move 16 | depth 8 | flag 8 | gen 8
} TTEntry;

typedef struct {
    int value, move, depth, flag;
} TTData;

enum { TT_NONE = 0, TT_UPPER = 1, TT_LOWER = 2, TT_EXACT = 3 };

static TTEntry *tt;
static size_t tt_mask;      // バケット数 - 1 (1 バケット = 2 エントリ)
static uint8_t tt_gen;
static int tt_mb = 32;
static int tt_used;         // 確保してから探索に使ったか

void search_set_tt_size(int mb) {
    if (mb < 1) mb = 1;
    if (tt && mb == tt_mb) return;
    tt_mb = mb;
    free(tt);
    tt = NULL;
    tt_used = 0;
}

static void tt_alloc(void) {
    if (tt) return;
    size_t n = 1;
    while (n * 2 * sizeof(TTEntry) * 2 <= (size_t)tt_mb << 20) n *= 2;
    tt = calloc(n * 2, sizeof(TTEntry));
    tt_mask = n - 1;
}

static int value_to_tt(int v, int ply) {
    if (v > SCORE_WIN_MIN) return v + ply;
    if (v < -SCORE_WIN_MIN) return v - ply;
    return v;
}

static int value_from_tt(int v, int ply) {
    if (v > SCORE_WIN_MIN) return v - ply;
    if (v < -SCORE_WIN_MIN) return v + ply;
    return v;
}

static inline int d_value(uint64_t d) { return (int16_t)(d & 0xFFFF); }
static inline int d_move(uint64_t d) { return (int16_t)(d >> 16 & 0xFFFF); }
static inline int d_depth(uint64_t d) { return (int8_t)(d >> 32 & 0xFF); }
static inline int d_flag(uint64_t d) { return (int)(d >> 40 & 0xFF); }
static inline int d_gen(uint64_t d) { return (int)(d >> 48 & 0xFF); }

static inline uint64_t ld(const uint64_t *p) { return __atomic_load_n(p, __ATOMIC_RELAXED); }
static inline void st(uint64_t *p, uint64_t v) { __atomic_store_n(p, v, __ATOMIC_RELAXED); }

static int tt_probe(uint64_t key, TTData *out) {
    TTEntry *e = &tt[(key & tt_mask) * 2];
    for (int i = 0; i < 2; i++) {
        uint64_t d = ld(&e[i].data);
        if ((ld(&e[i].key) ^ d) == key && d_flag(d)) {
            out->value = d_value(d);
            out->move = d_move(d);
            out->depth = d_depth(d);
            out->flag = d_flag(d);
            return 1;
        }
    }
    return 0;
}

static void tt_store(uint64_t key, int value, int move, int depth, int flag, int ply) {
    TTEntry *e = &tt[(key & tt_mask) * 2];
    uint64_t d0 = ld(&e[0].data), d1 = ld(&e[1].data);
    int m0 = (ld(&e[0].key) ^ d0) == key, m1 = (ld(&e[1].key) ^ d1) == key;
    int slot;
    uint64_t old;
    if (m0) slot = 0, old = d0;
    else if (m1) slot = 1, old = d1;
    else {
        // 古い世代か浅いほうを置き換える
        int s0 = d_depth(d0) - (d_gen(d0) != tt_gen) * 64;
        int s1 = d_depth(d1) - (d_gen(d1) != tt_gen) * 64;
        slot = (s0 <= s1) ? 0 : 1;
        old = 0;
    }
    int same = m0 || m1;
    if (same && move < 0) move = d_move(old);   // 手の情報は残す
    if (same && d_gen(old) == tt_gen && depth < d_depth(old) - 2 && flag != TT_EXACT) return;
    if (depth < -100) depth = -100;
    if (depth > 100) depth = 100;
    uint64_t d = (uint64_t)(uint16_t)value_to_tt(value, ply) | (uint64_t)(uint16_t)move << 16 |
                 (uint64_t)(uint8_t)depth << 32 | (uint64_t)(uint8_t)flag << 40 | (uint64_t)tt_gen << 48;
    st(&e[slot].key, key ^ d);
    st(&e[slot].data, d);
}

// ---------------------------------------------------------------- 共通

#define MAX_THREADS 16

typedef struct {
    Board *b;
    long nodes;
    long max_nodes;
    int stop;
    int id;             // 0 = 主スレッド (時間を見る・手を決める)
    double t0;
    int hard_ms;
    int seldepth;
    int root_depth;
    int killer[MAX_PLY][2];
    int pv[MAX_PLY][MAX_PLY];
    int pvlen[MAX_PLY];
    int history[3][NCELL];  // 前の手の情報を少し残す (search_root で 1/4 に)
} Ctx;

static Ctx ctxs[MAX_THREADS];
static Board boards[MAX_THREADS];   // 補助スレッド用の盤のコピー
static int n_threads = 1;
static int g_stop;                  // 主スレッドが探索を終えたら 1 (補助スレッドを止める)

void search_set_threads(int n) {
    n_threads = n < 1 ? 1 : n > MAX_THREADS ? MAX_THREADS : n;
}

// 新しい対局: 置換表は局面のハッシュで引くので前の対局の内容も正しいまま使える (消さない)。
// 全体を消すと 128MB の確保直後は数百 ms かかる (ページの割り当て) ので、ルールや評価が変わった時だけ消す
void search_new_game(void) {
    tt_alloc();
    for (int i = 0; i < MAX_THREADS; i++) memset(ctxs[i].history, 0, sizeof ctxs[i].history);
}

void search_clear_tt(void) {
    if (tt && tt_used) memset(tt, 0, (tt_mask + 1) * 2 * sizeof(TTEntry));   // 確保したばかりなら 0 のまま
    tt_used = 0;
    tt_gen = 0;
}

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec * 1e-6;
}

static inline int restricted(const Board *b, int color) {
    return color == BLACK && rule_has_forbidden(&b->rule);
}

static inline int legal(Board *b, int color, int q) {
    return !restricted(b, color) || !board_is_forbidden(b, q);
}

// 盤上の空点で color のパターンが p4 の点を 1 つ探す (なければ -1)
static int find_cell(const Board *b, int color, int p4) {
    for (int y = 0; y < b->size; y++) {
        int q = to_pos(0, y);
        for (int x = 0; x < b->size; x++, q++)
            if (b->cell[q] == EMPTY && b->pat4[color][q] == p4) return q;
    }
    return -1;
}

// color が (禁じ手でなく) 打てる達四・四四の点があるか。あればその点、なければ -1
static int legal_flex4(Board *b, int color) {
    if (!b->p4cnt[color][P4_FLEX4]) return -1;
    for (int y = 0; y < b->size; y++) {
        int q = to_pos(0, y);
        for (int x = 0; x < b->size; x++, q++)
            if (b->cell[q] == EMPTY && b->pat4[color][q] == P4_FLEX4 && legal(b, color, q)) return q;
    }
    return -1;
}

int evaluate(const Board *b) {
    int me = board_side(b), op = opp_color(me);
    const EvalParams *w = &g_eval_params;
    long v = b->vsum[me] - (long)b->vsum[op] * w->opp_scale / 64 + w->tempo;
    const int (*th)[THREAT_COUNT] = w->threat[me == BLACK ? 0 : 1];
    int bm = eval_threat_bits(b->p4cnt[me]), bo = eval_threat_bits(b->p4cnt[op]);
    for (int k = 0; k < THREAT_COUNT; k++) {
        if (bm >> k & 1) v += th[0][k];
        if (bo >> k & 1) v += th[1][k];
    }
    if (v > 20000) v = 20000;
    if (v < -20000) v = -20000;
    return (int)v;
}

// 調整できる探索の値 (INFO p_<名前> <値> で変えられる。search_set_param)
static struct {
    int qdepth;     // 静止探索の深さの限度 (四と防ぎの手数)
    int q_min_p4;   // 静止探索で読む自分の四の最低の形 (P4_BLOCK4 / P4_BLOCK4_PLUS / P4_BLOCK4_FLEX3)
    int soft_pct;   // 上限時間の何 % を過ぎたら次の反復を始めないか (think.c が使う)
    int vct_pct;    // ルートの VCT に使う時間 (上限時間の %)。0 = 使わない
    int vct_depth;  // ルートの VCT の深さ (攻めの手数)
    int def_restrict;   // 1 = 相手に VCF があればルートの候補を vcf_defenses の手 (受けの上位集合) に絞る
    int lmp_mul;        // 浅い所の手数打ち切りの手数の倍率 (%)
    int lmr_moves;      // 何手目から深さを削るか
    int cand_min;       // ルート以外で、点 (攻め+守り) がこれ未満の戦術的でない手は 2 手目以降読まない
    int ext_three;      // 1 = 活三 (相手に受けを強いる手) は深さを減らさない (延長)。延長はルートの深さの 2 倍まで
    int vctdef_pct;     // 相手の VCT への受け (vct_defenses) に使う時間 (上限時間の %)。0 = 使わない
    int unstable_pct;   // 直前の反復で最善手が変わったら、次の反復を始めない目安を何 % に延ばすか
    int leaf_vcf;       // 末端 (深さ 0) で手番側に四があれば VCF を調べるノード数の上限。0 = 調べない
    int leaf_vcf_depth; // 末端の VCF の深さ (攻めの手数)
} sp = {0, P4_BLOCK4_PLUS, 60, 0, 10, 1, 100, 3, 0, 1, 0, 100, 0, 12};

int search_soft_pct(void) { return sp.soft_pct; }

int search_set_param(const char *name, long v) {
    if (!strcmp(name, "qdepth")) sp.qdepth = (int)v;
    else if (!strcmp(name, "q_min_p4")) sp.q_min_p4 = (int)v;
    else if (!strcmp(name, "soft_pct")) sp.soft_pct = (int)v;
    else if (!strcmp(name, "vct_pct")) sp.vct_pct = (int)v;
    else if (!strcmp(name, "vct_depth")) sp.vct_depth = (int)v;
    else if (!strcmp(name, "def_restrict")) sp.def_restrict = (int)v;
    else if (!strcmp(name, "lmp_mul")) sp.lmp_mul = (int)v;
    else if (!strcmp(name, "lmr_moves")) sp.lmr_moves = (int)v;
    else if (!strcmp(name, "cand_min")) sp.cand_min = (int)v;
    else if (!strcmp(name, "ext_three")) sp.ext_three = (int)v;
    else if (!strcmp(name, "vctdef_pct")) sp.vctdef_pct = (int)v;
    else if (!strcmp(name, "unstable_pct")) sp.unstable_pct = (int)v;
    else if (!strcmp(name, "threads")) search_set_threads((int)v);
    else if (!strcmp(name, "leaf_vcf")) sp.leaf_vcf = (int)v;
    else if (!strcmp(name, "leaf_vcf_depth")) sp.leaf_vcf_depth = (int)v;
    else if (!strcmp(name, "opp_scale")) g_eval_params.opp_scale = (int)v;
    else if (!strcmp(name, "tempo")) g_eval_params.tempo = (int)v;
    else if (!strncmp(name, "threat", 6) && strlen(name) == 9) {
        // threat<手番 0=黒 1=白><0=手番側 1=相手側><種類>  例: threat010
        int a = name[6] - '0', s2 = name[7] - '0', k = name[8] - '0';
        if (a < 0 || a > 1 || s2 < 0 || s2 > 1 || k < 0 || k >= THREAT_COUNT) return 0;
        g_eval_params.threat[a][s2][k] = (int)v;
    }
    else if (!strncmp(name, "line", 4) && atoi(name + 4) >= 0 && atoi(name + 4) < P_COUNT) g_eval_params.line[atoi(name + 4)] = (int)v;
    else if (!strncmp(name, "combo", 5) && atoi(name + 5) >= 0 && atoi(name + 5) < P4_COUNT) g_eval_params.combo[atoi(name + 5)] = (int)v;
    else return 0;
    return 1;
}

// ---------------------------------------------------------------- 手生成

typedef struct {
    int pos;
    int score;
    int value;      // パターン点 (攻め + 守り)。history などを含まない
    int tactical;   // 三・四を作る、または相手の三・四の点をふさぐ
} Move;

#define DEF_SCALE 52    // 守り (相手の点) の重み /64

// 相手の四・活三への防ぎ手か: 相手がそこに打つとその線で四以上になる点
static inline int blocks_threat(const Board *b, int op, int q) {
    const uint8_t *pt = b->pat[op][q];
    for (int d = 0; d < 4; d++)
        if (pt[d] >= P_B4 && pt[d] <= P_D4) return 1;
    return 0;
}

// mode: 0 = 通常 (近くの空点すべて。op_threat なら防ぎ手と自分の四だけ)、1 = 静止探索 (自分の四だけ)
static int gen_moves(Ctx *c, Move *out, int me, int op, int op_threat, int ttmove, int ply, int mode) {
    Board *b = c->b;
    int n = 0;
    if (b->nmoves == 0) {
        out[0].pos = to_pos(b->size / 2, b->size / 2);
        out[0].score = 0;
        out[0].tactical = 0;
        out[0].value = 0;
        return 1;
    }
    const int *k = ply < MAX_PLY ? c->killer[ply] : NULL;
    // 石のある範囲 ±2 だけを見る
    int x0 = b->size, x1 = -1, y0 = b->size, y1 = -1;
    for (int i = 0; i < b->nmoves; i++) {
        int x = pos_x(b->moves[i]), y = pos_y(b->moves[i]);
        if (x < x0) x0 = x;
        if (x > x1) x1 = x;
        if (y < y0) y0 = y;
        if (y > y1) y1 = y;
    }
    x0 = x0 > 2 ? x0 - 2 : 0, y0 = y0 > 2 ? y0 - 2 : 0;
    x1 = x1 + 2 < b->size ? x1 + 2 : b->size - 1, y1 = y1 + 2 < b->size ? y1 + 2 : b->size - 1;
    const int16_t *vme = g_cell_value[me], *vop = g_cell_value[op];
    const int *hist = c->history[me];
    for (int y = y0; y <= y1; y++) {
        int q = to_pos(x0, y);
        for (int x = x0; x <= x1; x++, q++) {
            if (b->cell[q] != EMPTY || !b->near[q]) continue;
            int a = b->pat4[me][q], d = b->pat4[op][q];
            if (mode == 1 && (a < sp.q_min_p4 || a > P4_BLOCK4_FLEX3)) continue;
            if (op_threat && a < P4_BLOCK4 && !blocks_threat(b, op, q)) continue;
            int s = vme[board_pidx(b, me, q)] + vop[board_pidx(b, op, q)] * DEF_SCALE / 64;
            out[n].value = s;
            s = s * 16 + hist[q] / 64;
            if (q == ttmove) s += 1 << 28;
            else if (k && (q == k[0] || q == k[1])) s += 1 << 12;
            out[n].pos = q;
            out[n].score = s;
            out[n].tactical = a >= P4_FLEX3 || d >= P4_FLEX3;
            n++;
        }
    }
    return n;
}

// 挿入ソート (降順)。ルート用
static void sort_moves(Move *m, int n) {
    for (int i = 1; i < n; i++) {
        Move t = m[i];
        int j = i - 1;
        while (j >= 0 && m[j].score < t.score) { m[j + 1] = m[j]; j--; }
        m[j + 1] = t;
    }
}

// i 番目以降で最も点の高い手を i 番目に持ってくる (枝刈りで早く打ち切ることが多いので全部は並べない)
static inline void pick_move(Move *m, int i, int n) {
    int bi = i;
    for (int j = i + 1; j < n; j++)
        if (m[j].score > m[bi].score) bi = j;
    if (bi != i) { Move t = m[i]; m[i] = m[bi]; m[bi] = t; }
}

// ---------------------------------------------------------------- PVS

static inline void check_time(Ctx *c) {
    if ((c->nodes & 1023) == 0) {
        if (c->id == 0 && now_ms() - c->t0 >= c->hard_ms) c->stop = 1;
        if (__atomic_load_n(&g_stop, __ATOMIC_RELAXED)) c->stop = 1;
    }
    if (c->max_nodes && c->nodes >= c->max_nodes) c->stop = 1;
}

// 浅い深さで、戦術的でない手を何手まで調べるか
static const int LMP_COUNT[5] = {0, 6, 10, 16, 24};


static int pvs(Ctx *c, int alpha, int beta, int depth, int ply, int is_pv) {
    Board *b = c->b;
    int me = board_side(b), op = opp_color(me);
    c->pvlen[ply] = 0;
    c->nodes++;
    check_time(c);
    if (c->stop) return 0;
    if (ply > c->seldepth) c->seldepth = ply;

    // 自分の五
    if (b->p4cnt[me][P4_FIVE]) return SCORE_WIN - ply;
    if (b->nmoves >= b->size * b->size) return 0;
    if (ply >= MAX_PLY - 2) return evaluate(b);

    // 詰みの距離による枝刈り
    if (alpha < -(SCORE_WIN - ply)) alpha = -(SCORE_WIN - ply);
    if (beta > SCORE_WIN - ply) beta = SCORE_WIN - ply;
    if (alpha >= beta) return alpha;

    // 相手の五の点: 止めるしかない (深さは減らさない)
    int opfive = b->p4cnt[op][P4_FIVE];
    if (opfive) {
        int q = find_cell(b, op, P4_FIVE);
        if (opfive >= 2 || !legal(b, me, q)) return -(SCORE_WIN - ply - 1);
        board_move(b, q);
        int v = -pvs(c, -beta, -alpha, depth, ply + 1, is_pv);
        board_undo(b);
        if (is_pv && !c->stop) {
            c->pv[ply][0] = q;
            memcpy(&c->pv[ply][1], c->pv[ply + 1], sizeof(int) * c->pvlen[ply + 1]);
            c->pvlen[ply] = c->pvlen[ply + 1] + 1;
        }
        return v;
    }
    // 自分の達四: 勝ち
    if (legal_flex4(b, me) >= 0) return SCORE_WIN - ply - 2;

    int op_threat = legal_flex4(b, op) >= 0;
    // 静止探索: 深さが尽きたら、相手の活三への防ぎ (脅威がある時) か自分の四だけを読む
    int qmode = depth <= 0, stand = -SCORE_INF;
    if (qmode) {
        // 末端の VCF: 評価の尺度を変えずに、手番側の四の連続による勝ちだけを見つける
        if (sp.leaf_vcf && depth == 0 &&
            b->p4cnt[me][P4_BLOCK4] + b->p4cnt[me][P4_BLOCK4_PLUS] + b->p4cnt[me][P4_BLOCK4_FLEX3] > 0) {
            TssResult r;
            if (tss_solve(b, me, TSS_VCF, sp.leaf_vcf_depth, sp.leaf_vcf, &r)) return SCORE_WIN - ply - r.nline;
        }
        if (depth <= -sp.qdepth) return evaluate(b);
        if (!op_threat) {
            stand = evaluate(b);
            if (stand >= beta) return stand;
            if (stand > alpha) alpha = stand;
        }
    }

    // 置換表
    int ttmove = -1;
    TTData e;
    if (tt_probe(b->hash, &e)) {
        ttmove = e.move;
        if (!is_pv && e.depth >= depth) {
            int v = value_from_tt(e.value, ply);
            if (e.flag == TT_EXACT || (e.flag == TT_LOWER && v >= beta) || (e.flag == TT_UPPER && v <= alpha))
                return v;
        }
    }

    Move list[MAX_MOVES];
    int n = gen_moves(c, list, me, op, op_threat, ttmove, ply, qmode && !op_threat);
    int best = stand, bestmove = -1, alpha0 = alpha, searched = 0;

    for (int i = 0; i < n; i++) {
        pick_move(list, i, n);
        int q = list[i].pos, tactical = list[i].tactical || op_threat;
        if (searched > 0 && !is_pv && !tactical && best > -SCORE_WIN_MIN) {
            if (depth >= 1 && depth <= 4 && searched * 100 >= LMP_COUNT[depth] * sp.lmp_mul) continue;
            if (list[i].value < sp.cand_min) continue;
        }
        if (restricted(b, me) && board_is_forbidden(b, q)) continue;

        int a4 = b->pat4[me][q];
        board_move(b, q);
        int v, nd = depth - 1;
        // 活三を作る手 (相手は受けるしかない) の延長
        if (sp.ext_three && !qmode && a4 >= P4_FLEX3 && a4 <= P4_FLEX3_2X && ply + depth < 2 * c->root_depth) nd = depth;
        if (searched == 0) {
            v = -pvs(c, -beta, -alpha, nd, ply + 1, is_pv);
        } else {
            int r = 0;
            if (!qmode && depth >= 3 && searched >= sp.lmr_moves && !tactical) {
                r = 1 + (searched >= 10) + (depth >= 8 && searched >= 20);
                if (r > nd - 1) r = nd - 1 > 0 ? nd - 1 : 0;
            }
            v = -pvs(c, -alpha - 1, -alpha, nd - r, ply + 1, 0);
            if (v > alpha && r) v = -pvs(c, -alpha - 1, -alpha, nd, ply + 1, 0);
            if (v > alpha && v < beta) v = -pvs(c, -beta, -alpha, nd, ply + 1, 1);
        }
        board_undo(b);
        if (c->stop) return 0;
        searched++;

        if (v > best) {
            best = v;
            bestmove = q;
            if (v > alpha) {
                alpha = v;
                if (is_pv) {
                    c->pv[ply][0] = q;
                    memcpy(&c->pv[ply][1], c->pv[ply + 1], sizeof(int) * c->pvlen[ply + 1]);
                    c->pvlen[ply] = c->pvlen[ply + 1] + 1;
                }
                if (v >= beta) {
                    if (!list[i].tactical && ply < MAX_PLY && c->killer[ply][0] != q) {
                        c->killer[ply][1] = c->killer[ply][0];
                        c->killer[ply][0] = q;
                    }
                    int *h = &c->history[me][q];
                    *h += depth * depth * 32;
                    if (*h > (1 << 20)) {
                        for (int cc = 1; cc <= 2; cc++)
                            for (int p = 0; p < NCELL; p++) c->history[cc][p] /= 2;
                    }
                    break;
                }
            }
        }
    }

    if (searched == 0) {
        // 打てる手がない: 相手の活三を止められない (負け)、または全部禁じ手 / 静止探索で四がない
        if (op_threat) best = -(SCORE_WIN - ply - 3);
        else if (qmode) best = stand;
        else best = evaluate(b);
    }

    int flag = best >= beta ? TT_LOWER : (best > alpha0 ? TT_EXACT : TT_UPPER);
    tt_store(b->hash, best, bestmove, depth, flag, ply);
    return best;
}

// ---------------------------------------------------------------- ルート

typedef struct {
    int pos;
    int score;      // 前の反復の点 (並べ替え用)
} RootMove;

#define ASPIRATION 60

static void log_info(const Ctx *c, int depth, int score, const int *pv, int pvlen, int verbose) {
    if (!verbose) return;
    double ms = now_ms() - c->t0;
    fprintf(stderr, "depth %2d sel %2d score %6d nodes %9ld nps %8.0f time %6.0f pv", depth, c->seldepth, score, c->nodes,
            ms > 0 ? c->nodes / ms * 1000 : 0, ms);
    for (int i = 0; i < pvlen && i < 12; i++) fprintf(stderr, " %d,%d", pos_x(pv[i]), pos_y(pv[i]));
    fprintf(stderr, "\n");
}

// Lazy SMP: 補助スレッドも同じ反復深化を (深さを少しずらして) 回し、置換表を通じて主スレッドを助ける
static const int SKIP_SIZE[20] = {1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4};
static const int SKIP_PHASE[20] = {0, 1, 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 7};

static void ctx_reset(Ctx *c, Board *b, int id, double t0) {
    c->b = b;
    c->id = id;
    c->nodes = 0;
    c->max_nodes = 0;
    c->stop = 0;
    c->t0 = t0;
    c->hard_ms = 1 << 30;
    c->seldepth = 0;
    c->root_depth = 0;
    memset(c->killer, 0, sizeof c->killer);
    memset(c->pvlen, 0, sizeof c->pvlen);
    // history は前の手の情報を少し残す
    for (int cc = 1; cc <= 2; cc++)
        for (int p = 0; p < NCELL; p++) c->history[cc][p] /= 4;
}

// 反復深化 (ルートの候補 root[0..nr) を調べ、結果を res に入れる)
static void iterate(Ctx *c, RootMove *root, int nr, const SearchLimits *lim, SearchResult *res) {
    Board *b = c->b;
    int max_depth = lim->max_depth > 0 ? lim->max_depth : MAX_PLY - 10;
    int prev_score = 0, prev_best = -1, unstable = 0;
    for (int depth = 1; depth <= max_depth; depth++) {
        if (c->id > 0) {
            // 補助スレッドはスレッドごとに一部の深さを飛ばす (同じ深さを全員で読まないように)
            int k = (c->id - 1) % 20;
            if ((depth + SKIP_PHASE[k]) / SKIP_SIZE[k] % 2) continue;
        }
        c->root_depth = depth;
        // aspiration window: 前の反復の点の周りの狭い窓で調べ、外れたら広げてやり直す
        int lo = -SCORE_INF, hi = SCORE_INF, best, bestmove, searched;
        int bestpv[MAX_PLY], bestpvlen;
        if (depth >= 4 && prev_score > -SCORE_WIN_MIN && prev_score < SCORE_WIN_MIN) {
            lo = prev_score - ASPIRATION;
            hi = prev_score + ASPIRATION;
        }
        for (;;) {
            int alpha = lo, beta = hi;
            best = -SCORE_INF, bestmove = -1, searched = 0, bestpvlen = 0;
            for (int i = 0; i < nr; i++) {
                int p = root[i].pos;
                board_move(b, p);
                int v;
                if (searched == 0) {
                    v = -pvs(c, -beta, -alpha, depth - 1, 1, 1);
                } else {
                    v = -pvs(c, -alpha - 1, -alpha, depth - 1, 1, 0);
                    if (v > alpha && v < beta && !c->stop) v = -pvs(c, -beta, -alpha, depth - 1, 1, 1);
                }
                board_undo(b);
                if (c->stop) break;
                searched++;
                root[i].score = v;
                if (v > best) {
                    best = v;
                    bestmove = p;
                    if (v > alpha) alpha = v;
                    bestpv[0] = p;
                    memcpy(&bestpv[1], c->pv[1], sizeof(int) * c->pvlen[1]);
                    bestpvlen = c->pvlen[1] + 1;
                    if (v > lo || searched == 1) {
                        // 反復の途中で打ち切られても、全幅で調べてより良かった手は使える
                        res->move = p;
                        res->score = v;
                        res->pvlen = bestpvlen;
                        memcpy(res->pv, bestpv, sizeof(int) * bestpvlen);
                    }
                    if (v >= beta) break;
                } else {
                    root[i].score = -SCORE_INF - i;   // null window で落ちた手は順位だけ
                }
            }
            if (c->stop) break;
            if (best <= lo && lo > -SCORE_INF) { lo = -SCORE_INF; continue; }
            if (best >= hi && hi < SCORE_INF) { hi = SCORE_INF; continue; }
            break;
        }
        if (c->stop && searched > 0 && best < -SCORE_WIN_MIN && searched < nr) {
            // 打ち切り時に、調べ終えた手が全部負けと分かっただけなら、まだ調べていない次の候補を打つ
            res->move = root[searched].pos;
            res->score = prev_score;
        }
        if (c->stop && searched == 0) break;
        if (!c->stop) {
            unstable = depth > 1 && bestmove != prev_best;
            prev_best = bestmove;
            res->depth = depth;
            prev_score = best;
            if (c->id == 0) log_info(c, depth, best, bestpv, bestpvlen, lim->verbose);
        }
        // 次の反復のために並べ替え (最善手を先頭、他は点の順。安定ソート)
        for (int i = 1; i < nr; i++) {
            RootMove m = root[i];
            int j = i - 1;
            while (j >= 0 && root[j].score < m.score) { root[j + 1] = root[j]; j--; }
            root[j + 1] = m;
        }
        if (c->stop) break;
        if (best > SCORE_WIN_MIN || best < -SCORE_WIN_MIN) {
            // 詰みが見えた: 少し深く確かめたら終わる
            if (depth >= 2 + (SCORE_WIN - (best > 0 ? best : -best))) break;
        }
        if (lim->soft_ms > 0 && now_ms() - c->t0 >= (double)lim->soft_ms * (unstable ? sp.unstable_pct : 100) / 100) break;
    }
}

typedef struct {
    Ctx *c;
    RootMove root[MAX_MOVES];
    int nr;
    SearchLimits lim;
    SearchResult res;
} Helper;

static Helper helpers[MAX_THREADS];

static void *helper_main(void *arg) {
    Helper *h = arg;
    tss_set_thread(h->c->id);
    iterate(h->c, h->root, h->nr, &h->lim, &h->res);
    return NULL;
}

void search_root(Board *b, const SearchLimits *lim, SearchResult *res) {
    Ctx *c = &ctxs[0];
    tt_alloc();
    tt_used = 1;
    tt_gen++;
    __atomic_store_n(&g_stop, 0, __ATOMIC_RELAXED);
    double t0 = now_ms();
    ctx_reset(c, b, 0, t0);
    c->hard_ms = lim->hard_ms > 0 ? lim->hard_ms : 1 << 30;
    c->max_nodes = lim->max_nodes;
    memset(res, 0, sizeof *res);
    res->move = -1;
    res->reason = "search";

    int me = board_side(b), op = opp_color(me);
    if (b->nmoves == 0) {
        res->move = to_pos(b->size / 2, b->size / 2);
        res->reason = "center";
        return;
    }
    // 即決の手
    int q = find_cell(b, me, P4_FIVE);
    if (q >= 0) { res->move = q; res->score = SCORE_WIN; res->reason = "five"; return; }
    if (b->p4cnt[op][P4_FIVE]) {
        q = find_cell(b, op, P4_FIVE);
        res->move = q;
        res->score = b->p4cnt[op][P4_FIVE] >= 2 || !legal(b, me, q) ? -SCORE_WIN : 0;
        res->reason = "block-five";
        if (b->p4cnt[op][P4_FIVE] >= 2 || legal(b, me, q)) {
            // 1 つなら止めるしかない。2 つ以上なら負け (どちらかを止める)
            return;
        }
        // 黒が止める点が禁じ手: 負けだが他の手を探す (下へ)
    }
    q = legal_flex4(b, me);
    if (q >= 0) { res->move = q; res->score = SCORE_WIN - 2; res->reason = "flex4"; return; }

    // 詰み探索 (vcf.c): 自分の VCF → 相手の VCF への受け → 自分の VCT
    // ノード数は持ち時間に比例させる (VCF・VCT の 1 ノードはおよそ 1.5us)
    long hard = lim->hard_ms > 0 ? lim->hard_ms : 1000;
    int vmove;
    if (vcf_solve(b, 40, hard * 60, &vmove)) {
        res->move = vmove;
        res->score = SCORE_WIN - 10;
        res->reason = "vcf";
        return;
    }
    static int defs[MAX_MOVES];
    int ndefs = vcf_defenses(b, hard * 60, defs);   // -1 = 相手に VCF なし
    if (ndefs < 0 && sp.vct_pct > 0 && vct_solve(b, sp.vct_depth, hard * sp.vct_pct * 6, &vmove)) {
        res->move = vmove;
        res->score = SCORE_WIN - 20;
        res->reason = "vct";
        return;
    }
    if (ndefs < 0 && sp.vctdef_pct > 0) {
        // 相手に VCT があれば、それを崩し得る手 (返されなかった手は負けと証明済み) に絞る
        ndefs = vct_defenses(b, sp.vct_depth, hard * sp.vctdef_pct * 6, defs);
        if (lim->verbose && ndefs >= 0) fprintf(stderr, "opponent has VCT: %d defenses\n", ndefs);
    }
    if (lim->verbose && ndefs >= 0) fprintf(stderr, "opponent has VCF: %d defenses\n", ndefs);

    // ルートの候補
    int op_threat = legal_flex4(b, op) >= 0;
    Move list[MAX_MOVES];
    TTData e;
    int n = gen_moves(c, list, me, op, op_threat, tt_probe(b->hash, &e) ? e.move : -1, 0, 0);
    sort_moves(list, n);
    RootMove root[MAX_MOVES];
    int nr = 0;
    // 相手の VCF への受け (vcf_defenses、受けになり得る手の上位集合) を先に調べる。def_restrict なら受けだけに絞る
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < n; i++) {
            int p = list[i].pos, isdef = 0;
            for (int k = 0; k < ndefs && !isdef; k++) isdef = defs[k] == p;
            if ((ndefs <= 0 || isdef == (pass == 0)) && legal(b, me, p)) root[nr++] = (RootMove){p, 0};
        }
        if (ndefs <= 0 || sp.def_restrict) break;
    }
    if (nr == 0 && ndefs > 0) {
        // 受けが候補の外 (遠い点) にしかない
        for (int k = 0; k < ndefs; k++)
            if (legal(b, me, defs[k])) root[nr++] = (RootMove){defs[k], 0};
    }
    if (nr == 0) {
        // 防ぎ手がない (相手の活三を止められない) か全部禁じ手: 何でもよいので合法手
        n = gen_moves(c, list, me, op, 0, -1, 0, 0);
        sort_moves(list, n);
        for (int i = 0; i < n; i++)
            if (legal(b, me, list[i].pos)) root[nr++] = (RootMove){list[i].pos, 0};
        if (nr == 0) {
            for (int y = 0; y < b->size && res->move < 0; y++)
                for (int x = 0; x < b->size; x++) {
                    int p = to_pos(x, y);
                    if (b->cell[p] == EMPTY && legal(b, me, p)) { res->move = p; break; }
                }
            res->reason = "no-move";
            return;
        }
        res->move = root[0].pos;
        res->score = -SCORE_WIN;
        res->reason = "lost";
        return;
    }
    res->move = root[0].pos;
    if (nr == 1) { res->reason = "only-move"; return; }

    pthread_t th[MAX_THREADS];
    int started[MAX_THREADS] = {0};
    for (int t = 1; t < n_threads; t++) {
        Helper *h = &helpers[t];
        boards[t] = *b;
        h->c = &ctxs[t];
        ctx_reset(h->c, &boards[t], t, t0);
        memcpy(h->root, root, sizeof(RootMove) * nr);
        h->nr = nr;
        h->lim = *lim;
        h->lim.soft_ms = 0;
        h->lim.verbose = 0;
        started[t] = pthread_create(&th[t], NULL, helper_main, h) == 0;
    }
    iterate(c, root, nr, lim, res);
    __atomic_store_n(&g_stop, 1, __ATOMIC_RELAXED);
    res->nodes = c->nodes;
    for (int t = 1; t < n_threads; t++) {
        if (!started[t]) continue;
        pthread_join(th[t], NULL);
        res->nodes += ctxs[t].nodes;
    }
    res->seldepth = c->seldepth;
    res->ms = (int)(now_ms() - c->t0);
}
