// 本探索
//
// - 反復深化 + PVS (null window で調べて、良ければ全幅で再探索)
// - 置換表 (Zobrist ハッシュ、2 エントリのバケット、深さ優先 + 世代で置き換え)
// - 強制手: 相手に五の点があれば止める手だけ (深さを減らさない = 四への延長)。
//   相手に達四の点 (活三など) があれば、防ぎ手と自分の四だけ
// - 手の並べ替え: 置換表の手 → パターン点 (攻め + 守り) + killer + history
// - 遅い手の深さ削減 (LMR) と浅い所での手数打ち切り (戦術的でない手のみ)
#include "search.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "eval.h"

// ---------------------------------------------------------------- 置換表

typedef struct {
    uint64_t key;
    int16_t value;
    int16_t move;
    int8_t depth;
    uint8_t flag;
    uint8_t gen;
    uint8_t pad;
} TTEntry;

enum { TT_NONE = 0, TT_UPPER = 1, TT_LOWER = 2, TT_EXACT = 3 };

static TTEntry *tt;
static size_t tt_mask;      // バケット数 - 1 (1 バケット = 2 エントリ)
static uint8_t tt_gen;
static int tt_mb = 32;

void search_set_tt_size(int mb) {
    if (mb < 1) mb = 1;
    if (tt && mb == tt_mb) return;
    tt_mb = mb;
    free(tt);
    tt = NULL;
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

static TTEntry *tt_probe(uint64_t key) {
    TTEntry *e = &tt[(key & tt_mask) * 2];
    if (e[0].key == key && e[0].flag) return &e[0];
    if (e[1].key == key && e[1].flag) return &e[1];
    return NULL;
}

static void tt_store(uint64_t key, int value, int move, int depth, int flag, int ply) {
    TTEntry *e = &tt[(key & tt_mask) * 2], *slot;
    if (e[0].key == key) slot = &e[0];
    else if (e[1].key == key) slot = &e[1];
    else {
        // 古い世代か浅いほうを置き換える
        int s0 = e[0].depth - (e[0].gen != tt_gen) * 64;
        int s1 = e[1].depth - (e[1].gen != tt_gen) * 64;
        slot = (s0 <= s1) ? &e[0] : &e[1];
    }
    if (slot->key == key && move < 0) move = slot->move;   // 手の情報は残す
    if (slot->key == key && slot->gen == tt_gen && depth < slot->depth - 2 && flag != TT_EXACT) return;
    slot->key = key;
    slot->value = (int16_t)value_to_tt(value, ply);
    slot->move = (int16_t)move;
    slot->depth = (int8_t)(depth < -100 ? -100 : depth > 100 ? 100 : depth);
    slot->flag = (uint8_t)flag;
    slot->gen = tt_gen;
}

// ---------------------------------------------------------------- 共通

static int history[3][NCELL];

void search_new_game(void) {
    tt_alloc();
    memset(tt, 0, (tt_mask + 1) * 2 * sizeof(TTEntry));
    memset(history, 0, sizeof history);
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
    long v = b->vsum[me] - (long)b->vsum[op] * g_eval_params.opp_scale / 64 + g_eval_params.tempo;
    if (v > 20000) v = 20000;
    if (v < -20000) v = -20000;
    return (int)v;
}

// ---------------------------------------------------------------- 簡易 VCF

static int vcf_rec(Board *b, int depth, long *budget);

// 手番側が q に四を打ち、相手が止めた後を調べる
static int vcf_after_four(Board *b, int q, int depth, long *budget) {
    int me = board_side(b), op = opp_color(me);
    board_move(b, q);
    int r;
    if (b->p4cnt[me][P4_FIVE] >= 2) {
        r = 1;
    } else {
        int f = find_cell(b, me, P4_FIVE);
        if (f < 0) {
            r = 0;
        } else if (restricted(b, op) && board_is_forbidden(b, f)) {
            r = 1;      // 黒は止める点が禁じ手で止められない
        } else {
            board_move(b, f);
            r = vcf_rec(b, depth - 1, budget);
            board_undo(b);
        }
    }
    board_undo(b);
    return r;
}

static int vcf_rec(Board *b, int depth, long *budget) {
    int me = board_side(b), op = opp_color(me);
    if (b->p4cnt[me][P4_FIVE]) return 1;
    if (depth <= 0 || --*budget < 0) return 0;
    if (b->p4cnt[op][P4_FIVE]) {
        // 相手の四を止める手が自分の四になっていれば続けられる
        if (b->p4cnt[op][P4_FIVE] >= 2) return 0;
        int q = find_cell(b, op, P4_FIVE);
        if (b->pat4[me][q] < P4_BLOCK4 || !legal(b, me, q)) return 0;
        return vcf_after_four(b, q, depth, budget);
    }
    if (legal_flex4(b, me) >= 0) return 1;
    for (int y = 0; y < b->size; y++) {
        int q = to_pos(0, y);
        for (int x = 0; x < b->size; x++, q++) {
            if (b->cell[q] != EMPTY) continue;
            int p4 = b->pat4[me][q];
            if (p4 < P4_BLOCK4 || p4 > P4_BLOCK4_FLEX3) continue;
            if (!legal(b, me, q)) continue;
            if (vcf_after_four(b, q, depth, budget)) return 1;
            if (*budget < 0) return 0;
        }
    }
    return 0;
}

int quick_vcf(Board *b, int max_depth, long max_nodes, int *win_move) {
    int me = board_side(b), op = opp_color(me);
    long budget = max_nodes;
    *win_move = -1;
    int f = find_cell(b, me, P4_FIVE);
    if (f >= 0) { *win_move = f; return 1; }
    if (b->p4cnt[op][P4_FIVE]) {
        if (b->p4cnt[op][P4_FIVE] >= 2) return 0;
        int q = find_cell(b, op, P4_FIVE);
        if (b->pat4[me][q] < P4_BLOCK4 || !legal(b, me, q)) return 0;
        if (vcf_after_four(b, q, max_depth, &budget)) { *win_move = q; return 1; }
        return 0;
    }
    int q = legal_flex4(b, me);
    if (q >= 0) { *win_move = q; return 1; }
    for (int y = 0; y < b->size; y++) {
        q = to_pos(0, y);
        for (int x = 0; x < b->size; x++, q++) {
            if (b->cell[q] != EMPTY) continue;
            int p4 = b->pat4[me][q];
            if (p4 < P4_BLOCK4 || p4 > P4_BLOCK4_FLEX3 || !legal(b, me, q)) continue;
            if (vcf_after_four(b, q, max_depth, &budget)) { *win_move = q; return 1; }
            if (budget < 0) return 0;
        }
    }
    return 0;
}

// ---------------------------------------------------------------- 手生成

typedef struct {
    int pos;
    int score;
    int tactical;   // 三・四を作る、または相手の三・四の点をふさぐ
} Move;

typedef struct {
    Board *b;
    long nodes;
    long max_nodes;
    int stop;
    double t0;
    int hard_ms;
    int seldepth;
    int killer[MAX_PLY][2];
    int pv[MAX_PLY][MAX_PLY];
    int pvlen[MAX_PLY];
} Ctx;

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
        return 1;
    }
    const int *k = ply < MAX_PLY ? c->killer[ply] : NULL;
    for (int y = 0; y < b->size; y++) {
        int q = to_pos(0, y);
        for (int x = 0; x < b->size; x++, q++) {
            if (b->cell[q] != EMPTY || !b->near[q]) continue;
            int a = b->pat4[me][q], d = b->pat4[op][q];
            if (mode == 1 && (a < P4_BLOCK4 || a > P4_BLOCK4_FLEX3)) continue;
            if (op_threat && a < P4_BLOCK4 && !blocks_threat(b, op, q)) continue;
            int s = g_cell_value[me][board_pidx(b, me, q)] + g_cell_value[op][board_pidx(b, op, q)] * DEF_SCALE / 64;
            s = s * 16 + history[me][q] / 64;
            if (q == ttmove) s += 1 << 28;
            else if (k && (q == k[0] || q == k[1])) s += 1 << 12;
            out[n].pos = q;
            out[n].score = s;
            out[n].tactical = a >= P4_FLEX3 || d >= P4_FLEX3;
            n++;
        }
    }
    // 挿入ソート (降順)
    for (int i = 1; i < n; i++) {
        Move m = out[i];
        int j = i - 1;
        while (j >= 0 && out[j].score < m.score) { out[j + 1] = out[j]; j--; }
        out[j + 1] = m;
    }
    return n;
}

// ---------------------------------------------------------------- PVS

static inline void check_time(Ctx *c) {
    if ((c->nodes & 1023) == 0) {
        if (now_ms() - c->t0 >= c->hard_ms) c->stop = 1;
    }
    if (c->max_nodes && c->nodes >= c->max_nodes) c->stop = 1;
}

// 浅い深さで、戦術的でない手を何手まで調べるか
static const int LMP_COUNT[5] = {0, 6, 10, 16, 24};
#define QDEPTH 12   // 静止探索の深さの限度 (四と防ぎの手数)

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
        if (depth <= -QDEPTH) return evaluate(b);
        if (!op_threat) {
            stand = evaluate(b);
            if (stand >= beta) return stand;
            if (stand > alpha) alpha = stand;
        }
    }

    // 置換表
    int ttmove = -1;
    TTEntry *e = tt_probe(b->hash);
    if (e) {
        ttmove = e->move;
        if (!is_pv && e->depth >= depth) {
            int v = value_from_tt(e->value, ply);
            if (e->flag == TT_EXACT || (e->flag == TT_LOWER && v >= beta) || (e->flag == TT_UPPER && v <= alpha))
                return v;
        }
    }

    Move list[MAX_MOVES];
    int n = gen_moves(c, list, me, op, op_threat, ttmove, ply, qmode && !op_threat);
    int best = stand, bestmove = -1, alpha0 = alpha, searched = 0;

    for (int i = 0; i < n; i++) {
        int q = list[i].pos, tactical = list[i].tactical || op_threat;
        if (searched > 0 && !is_pv && !tactical && depth >= 1 && depth <= 4 && searched >= LMP_COUNT[depth] &&
            best > -SCORE_WIN_MIN)
            continue;
        if (restricted(b, me) && board_is_forbidden(b, q)) continue;

        board_move(b, q);
        int v, nd = depth - 1;
        if (searched == 0) {
            v = -pvs(c, -beta, -alpha, nd, ply + 1, is_pv);
        } else {
            int r = 0;
            if (!qmode && depth >= 3 && searched >= 3 && !tactical) {
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
                    int *h = &history[me][q];
                    *h += depth * depth * 32;
                    if (*h > (1 << 20)) {
                        for (int cc = 1; cc <= 2; cc++)
                            for (int p = 0; p < NCELL; p++) history[cc][p] /= 2;
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

static void log_info(const Ctx *c, int depth, int score, const int *pv, int pvlen, int verbose) {
    if (!verbose) return;
    double ms = now_ms() - c->t0;
    fprintf(stderr, "depth %2d sel %2d score %6d nodes %9ld nps %8.0f time %6.0f pv", depth, c->seldepth, score, c->nodes,
            ms > 0 ? c->nodes / ms * 1000 : 0, ms);
    for (int i = 0; i < pvlen && i < 12; i++) fprintf(stderr, " %d,%d", pos_x(pv[i]), pos_y(pv[i]));
    fprintf(stderr, "\n");
}

void search_root(Board *b, const SearchLimits *lim, SearchResult *res) {
    static Ctx ctx;
    Ctx *c = &ctx;
    tt_alloc();
    tt_gen++;
    memset(c, 0, sizeof *c);
    c->b = b;
    c->t0 = now_ms();
    c->hard_ms = lim->hard_ms > 0 ? lim->hard_ms : 1 << 30;
    c->max_nodes = lim->max_nodes;
    memset(res, 0, sizeof *res);
    res->move = -1;
    res->reason = "search";

    int me = board_side(b), op = opp_color(me);
    // history は前の手の情報を少し残す
    for (int cc = 1; cc <= 2; cc++)
        for (int p = 0; p < NCELL; p++) history[cc][p] /= 4;

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

    // 自分の VCF
    int vmove;
    if (quick_vcf(b, 40, 200000, &vmove)) {
        res->move = vmove;
        res->score = SCORE_WIN - 10;
        res->reason = "vcf";
        if (lim->verbose) fprintf(stderr, "vcf found %d,%d\n", pos_x(vmove), pos_y(vmove));
        return;
    }

    // ルートの候補
    int op_threat = legal_flex4(b, op) >= 0;
    Move list[MAX_MOVES];
    TTEntry *e = tt_probe(b->hash);
    int n = gen_moves(c, list, me, op, op_threat, e ? e->move : -1, 0, 0);
    RootMove root[MAX_MOVES];
    int nr = 0;
    for (int i = 0; i < n; i++)
        if (legal(b, me, list[i].pos)) root[nr++] = (RootMove){list[i].pos, 0};
    if (nr == 0) {
        // 防ぎ手がない (相手の活三を止められない) か全部禁じ手: 何でもよいので合法手
        n = gen_moves(c, list, me, op, 0, -1, 0, 0);
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

    int max_depth = lim->max_depth > 0 ? lim->max_depth : MAX_PLY - 10;
    int prev_score = 0;
    for (int depth = 1; depth <= max_depth; depth++) {
        int alpha = -SCORE_INF, beta = SCORE_INF, best = -SCORE_INF, bestmove = -1;
        int searched = 0;
        int bestpv[MAX_PLY], bestpvlen = 0;
        for (int i = 0; i < nr; i++) {
            int p = root[i].pos;
            board_move(b, p);
            int v;
            if (searched == 0) {
                v = -pvs(c, -beta, -alpha, depth - 1, 1, 1);
            } else {
                v = -pvs(c, -alpha - 1, -alpha, depth - 1, 1, 0);
                if (v > alpha && !c->stop) v = -pvs(c, -beta, -alpha, depth - 1, 1, 1);
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
                // 反復の途中で打ち切られても、全幅で調べてより良かった手は使える
                res->move = p;
                res->score = v;
                res->pvlen = bestpvlen;
                memcpy(res->pv, bestpv, sizeof(int) * bestpvlen);
            } else {
                root[i].score = -SCORE_INF + i;   // null window で落ちた手は順位だけ
            }
        }
        if (c->stop && searched == 0) break;
        if (!c->stop) {
            res->depth = depth;
            prev_score = best;
            log_info(c, depth, best, bestpv, bestpvlen, lim->verbose);
            (void)bestmove;
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
        if (lim->soft_ms > 0 && now_ms() - c->t0 >= lim->soft_ms) break;
    }
    (void)prev_score;
    res->nodes = c->nodes;
    res->seldepth = c->seldepth;
    res->ms = (int)(now_ms() - c->t0);
}
