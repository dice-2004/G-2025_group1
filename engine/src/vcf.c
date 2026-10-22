// VCF / VCT 詰み探索 (説明は vcf.h)
//
// 攻め側の手番の局面 (OR) と受け側の手番の局面 (AND) を交互に調べる深さ優先探索。
// 攻め側の局面だけ置換表に「勝ち」「深さ d までは勝ちなし」を覚える。
// 盤のハッシュは手番を含まないが、攻め側の局面は必ず攻め側の手番なので、
// (モード, 攻め側の色) ごとにハッシュをずらせば混ざらない。
#include "vcf.h"

#include <stdlib.h>
#include <string.h>

#define TT_BITS 20
#define TT_SIZE (1u << TT_BITS)
int g_vcf_inner = 24;     // VCT の各局面で先に試す VCF の深さ
#define VCF_INNER_DEPTH g_vcf_inner
#define MAX_CAND 128

enum { TT_NONE = 0, TT_WIN = 1, TT_FAIL = 2 };

int g_zone_k = 2;   // VCT の三を探す範囲: 探索中の攻め側の直近何手の周りか (0 = 盤全体)

typedef struct {
    uint64_t key;
    int16_t depth;      // TT_FAIL: この深さまでは勝ちなし
    int16_t best;       // TT_WIN: 勝ちの手
    int8_t result;
} TTEntry;

static TTEntry *tt;
static const uint64_t SALT[2][3] = {
    {0, 0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL},
    {0, 0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL},
};

static struct {
    Board *b;
    int att, def;
    int restricted;     // 黒に禁じ手がある
    long nodes, max_nodes;
    int aborted;
    int root_nmoves, root_best;
    int npos;
    int pos[MAX_MOVES]; // 盤上のマス
} C;

void tss_clear(void) {
    if (tt) memset(tt, 0, sizeof(TTEntry) * TT_SIZE);
}

static void tt_store(uint64_t key, int depth, int win, int best) {
    if (C.aborted) return;
    TTEntry *e = &tt[key & (TT_SIZE - 1)];
    // 勝ちの記録は同じ局面の負けの記録で上書きしない
    if (e->key == key && e->result == TT_WIN && !win) return;
    e->key = key;
    e->depth = (int16_t)depth;
    e->best = (int16_t)best;
    e->result = win ? TT_WIN : TT_FAIL;
}

static inline int legal(int color, int p) {
    return !(color == BLACK && C.restricted && board_is_forbidden(C.b, p));
}

// color が打てば五になる点を最大 max 個 (五は禁じ手より優先なので合法性の確認は不要)
static int find_fives(int color, int *out, int max) {
    const Board *b = C.b;
    int n = 0;
    for (int i = 0; i < C.npos; i++) {
        int p = C.pos[i];
        if (b->cell[p] == EMPTY && b->pat4[color][p] == P4_FIVE) {
            out[n++] = p;
            if (n >= max) break;
        }
    }
    return n;
}

// m を通る 4 方向 ±4 マスにある color の五の点 (m に石が置かれた直後の新しい五の点はここだけ)
static int fives_near(int color, int m, int *out, int max) {
    const Board *b = C.b;
    int n = 0;
    for (int d = 0; d < 4; d++) {
        int off = DIR_OFFSET[d];
        for (int k = -4; k <= 4; k++) {
            int q = m + k * off;
            if (k == 0 || q < 0 || q >= NCELL || b->cell[q] != EMPTY || b->pat4[color][q] != P4_FIVE) continue;
            int dup = 0;
            for (int i = 0; i < n; i++) dup |= out[i] == q;
            if (dup) continue;
            out[n++] = q;
            if (n >= max) return n;
        }
    }
    return n;
}

static int chebyshev(int p, int q) {
    int dx = abs(pos_x(p) - pos_x(q)), dy = abs(pos_y(p) - pos_y(q));
    return dx > dy ? dx : dy;
}

// 直近の手の近くを優先するための点
static int near_bonus(int p) {
    const Board *b = C.b;
    int s = 0;
    for (int k = 1; k <= 2 && k <= b->nmoves; k++) {
        int d = chebyshev(p, b->moves[b->nmoves - k]);
        if (d < 6) s += 6 - d;
    }
    return s;
}

static void sort_desc(int *mv, int *sc, int n) {
    for (int i = 1; i < n; i++) {
        int m = mv[i], s = sc[i], j = i - 1;
        while (j >= 0 && sc[j] < s) { mv[j + 1] = mv[j]; sc[j + 1] = sc[j]; j--; }
        mv[j + 1] = m;
        sc[j + 1] = s;
    }
}

static int is_three4(int p4) { return p4 >= P4_FLEX3 && p4 <= P4_FLEX3_2X; }

static int attacker_node(int mode, int depth);
static int defender_node(int mode, int depth);

static int win_at(uint64_t key, int depth, int best) {
    tt_store(key, depth, 1, best);
    if (C.b->nmoves == C.root_nmoves) C.root_best = best;
    return 1;
}

// 攻め側の手番。勝ちを証明できれば 1
static int attacker_node(int mode, int depth) {
    Board *b = C.b;
    if (++C.nodes > C.max_nodes) { C.aborted = 1; return 0; }
    int f[2];
    uint64_t key = b->hash ^ SALT[mode][C.att];
    int ndef;
    if (b->nmoves == C.root_nmoves) {
        if (find_fives(C.att, f, 1)) return win_at(key, depth, f[0]);
        ndef = find_fives(C.def, f, 2);
    } else {
        // 探索中は攻め側に五の点は残らない (あれば打っている)。受け側の五の点は直前の手の周りだけ
        ndef = fives_near(C.def, b->moves[b->nmoves - 1], f, 2);
    }
    if (ndef >= 2) return 0;

    TTEntry *e = &tt[key & (TT_SIZE - 1)];
    if (e->key == key) {
        if (e->result == TT_WIN) {
            if (b->nmoves == C.root_nmoves) C.root_best = e->best;
            return 1;
        }
        if (e->depth >= depth) return 0;
    }

    if (ndef == 1) {
        // 受け側の四を止めるしかない。止める手が攻めの脅威になっていれば続く
        int q = f[0];
        if (depth <= 0 || !legal(C.att, q)) { tt_store(key, depth, 0, -1); return 0; }
        board_move_color(b, q, C.att);
        int r = defender_node(mode, depth - 1);
        board_undo(b);
        if (r) return win_at(key, depth, q);
        tt_store(key, depth, 0, -1);
        return 0;
    }

    // VCT の三は、探索中に打った攻め側の直近の手の近く (4 方向 ±4 と周囲 2 マス) だけ
    static int zone[NCELL], zgen = 0;
    int zoned = mode == TSS_VCT && g_zone_k > 0 && b->nmoves - C.root_nmoves >= 2;
    if (zoned) {
        zgen++;
        for (int k = 0, i = b->nmoves - 2; k < g_zone_k && i >= C.root_nmoves; k++, i -= 2) {
            int m = b->moves[i];
            for (int d = 0; d < 4; d++)
                for (int j = -4; j <= 4; j++) {
                    int q = m + j * DIR_OFFSET[d];
                    if (q >= 0 && q < NCELL) zone[q] = zgen;
                }
            for (int dy = -2; dy <= 2; dy++)
                for (int dx = -2; dx <= 2; dx++) {
                    int q = m + dy * STRIDE + dx;
                    if (q >= 0 && q < NCELL) zone[q] = zgen;
                }
        }
    }

    // 達四 (止められない四) が打てれば勝ち
    // 盤の集計 (形ごとの空点の数) で、探す形の点を全部見つけたら走査をやめる
    int cand[MAX_CAND], score[MAX_CAND], nc = 0;
    const int16_t *cnt = b->p4cnt[C.att];
    int want = cnt[P4_BLOCK4] + cnt[P4_BLOCK4_PLUS] + cnt[P4_BLOCK4_FLEX3] + cnt[P4_FLEX4];
    if (mode == TSS_VCT) want += cnt[P4_FLEX3] + cnt[P4_FLEX3_PLUS] + cnt[P4_FLEX3_2X];
    for (int i = 0; i < C.npos && want > 0; i++) {
        int p = C.pos[i];
        if (b->cell[p] != EMPTY) continue;
        int p4 = b->pat4[C.att][p];
        if (p4 < P4_FLEX3 || p4 == P4_FIVE || (mode == TSS_VCF && p4 < P4_BLOCK4)) continue;
        want--;
        if (p4 == P4_FLEX4) {
            if (legal(C.att, p)) return win_at(key, depth, p);
            continue;
        }
        if (p4 >= P4_BLOCK4 || (mode == TSS_VCT && is_three4(p4) && (!zoned || zone[p] == zgen))) {
            if (nc < MAX_CAND) {
                cand[nc] = p;
                score[nc] = p4 * 32 + near_bonus(p) + (p4 >= P4_BLOCK4 ? 512 : 0);

                nc++;
            }
        }
    }
    if (depth <= 0) return 0;

    if (mode == TSS_VCT) {
        // まず VCF で勝てるか (安く調べられる)
        if (attacker_node(TSS_VCF, VCF_INNER_DEPTH)) {
            TTEntry *v = &tt[(b->hash ^ SALT[TSS_VCF][C.att]) & (TT_SIZE - 1)];
            int best = (v->key == (b->hash ^ SALT[TSS_VCF][C.att]) && v->result == TT_WIN) ? v->best : -1;
            if (best < 0 && b->nmoves == C.root_nmoves) best = C.root_best;
            return win_at(key, depth, best);
        }
        if (C.aborted) return 0;
    }

    sort_desc(cand, score, nc);
    for (int i = 0; i < nc; i++) {
        int p = cand[i];
        if (!legal(C.att, p)) continue;
        board_move_color(b, p, C.att);
        int r = defender_node(mode, depth - 1);
        board_undo(b);
        if (r) return win_at(key, depth, p);
        if (C.aborted) return 0;
    }
    tt_store(key, depth, 0, -1);
    return 0;
}

// 活三の受けの候補: 攻め側の達四点 t すべてについて、t と、t で四以上になる方向の ±5 マスの空点の共通部分。
// ここに入らない手 (四でもない) を受け側が打つと、残った達四点で攻め側が勝つ
static int three_defenses(const int *t, int nt, int *out) {
    static int stamp[NCELL], cgen[NCELL], cnt[NCELL];
    static int gen = 0, call = 0;
    const Board *b = C.b;
    int n = 0;
    call++;
    for (int i = 0; i < nt; i++) {
        gen++;
        int lst[64], nl = 0;
        lst[nl++] = t[i];
        stamp[t[i]] = gen;
        for (int d = 0; d < 4; d++) {
            if (b->pat[C.att][t[i]][d] < P_B4) continue;
            int off = DIR_OFFSET[d];
            for (int k = -5; k <= 5; k++) {
                int q = t[i] + k * off;
                if (k == 0 || q < 0 || q >= NCELL || b->cell[q] != EMPTY || stamp[q] == gen) continue;
                stamp[q] = gen;
                lst[nl++] = q;
            }
        }
        for (int j = 0; j < nl; j++) {
            int q = lst[j];
            if (cgen[q] != call) { cgen[q] = call; cnt[q] = 0; }
            if (cnt[q] == i) cnt[q] = i + 1;
            if (i == nt - 1 && cnt[q] == nt) out[n++] = q;
        }
    }
    return n;
}

// 受け側の手番で、受け側がパスすれば攻め側に VCF がある (置換表に勝ちが入っている) とき、
// その VCF 手順を崩し得る点をすべて out に入れる: 手順上の点、最後の五の点、
// 受けた点の 4 方向 ±4 (受け側の逆四の準備)。止め点が黒の禁じ手で勝つ手順なら空点すべて。
// ここに入らない手 (四でもない) を受け側が打っても同じ手順で攻め側が勝つ。手順をたどれなければ -1
static int vcf_threat_cells(int *out) {
    static int stamp[NCELL];
    static int gen = 0;
    Board *b = C.b;
    int n = 0, played = 0, ok = 0;
    gen++;
#define ADD_CELL(q) do { int q_ = (q); \
        if (q_ >= 0 && q_ < NCELL && b->cell[q_] == EMPTY && stamp[q_] != gen) { stamp[q_] = gen; out[n++] = q_; } } while (0)
#define ADD_LINES(c) do { for (int d_ = 0; d_ < 4; d_++) for (int k_ = -4; k_ <= 4; k_++) ADD_CELL((c) + k_ * DIR_OFFSET[d_]); } while (0)
    for (int step = 0; step < 64; step++) {
        int f[2], a;
        int nd = find_fives(C.def, f, 2);
        if (nd >= 2) break;
        if (nd == 1) {
            a = f[0];
        } else {
            uint64_t key = b->hash ^ SALT[TSS_VCF][C.att];
            TTEntry *e = &tt[key & (TT_SIZE - 1)];
            if (e->key != key || e->result != TT_WIN || e->best < 0) break;
            a = e->best;
        }
        if (b->cell[a] != EMPTY) break;
        ADD_CELL(a);
        if (b->pat4[C.att][a] == P4_FIVE) { ok = 1; break; }
        board_move_color(b, a, C.att);
        played++;
        int na = fives_near(C.att, a, f, 2);
        if (na == 0) break;
        if (na >= 2) { ADD_CELL(f[0]); ADD_CELL(f[1]); ok = 1; break; }
        if (!legal(C.def, f[0])) {
            // 止め点が黒の禁じ手で勝つ: 禁じ手かどうかは偽の三の再帰で遠くの石にも左右されるので空点すべて
            for (int i = 0; i < C.npos; i++) ADD_CELL(C.pos[i]);
            ok = 1;
            break;
        }
        ADD_CELL(f[0]);
        ADD_LINES(f[0]);
        board_move_color(b, f[0], C.def);
        played++;
    }
#undef ADD_CELL
#undef ADD_LINES
    while (played--) board_undo(b);
    return ok ? n : -1;
}

// 受け側の手番 (直前に攻め側が脅威を打った)。攻め側の勝ちを証明できれば 1
static int defender_node(int mode, int depth) {
    Board *b = C.b;
    if (++C.nodes > C.max_nodes) { C.aborted = 1; return 0; }
    // 受け側の五の点は攻め側の局面で止めさせているので残っていない。攻め側の五の点は直前の手の周りだけ
    int f[2];
    int natt = fives_near(C.att, b->moves[b->nmoves - 1], f, 2);
    if (natt >= 2) return 1;
    if (natt == 1) {
        int q = f[0];
        if (!legal(C.def, q)) return 1;     // 止め点が黒の禁じ手
        board_move_color(b, q, C.def);
        int r = attacker_node(mode, depth);
        board_undo(b);
        return r;
    }
    if (mode == TSS_VCF) return 0;

    // 活三の脅威: 攻め側の (打てる) 達四点
    int t[MAX_CAND], nt = 0;
    int fours[MAX_CAND], nf = 0;
    for (int i = 0; i < C.npos; i++) {
        int p = C.pos[i];
        if (b->cell[p] != EMPTY) continue;
        if (b->pat4[C.att][p] == P4_FLEX4 && nt < MAX_CAND && legal(C.att, p)) t[nt++] = p;
        if (b->pat4[C.def][p] >= P4_BLOCK4 && nf < MAX_CAND) fours[nf++] = p;
    }
    int cand[MAX_MOVES], score[MAX_MOVES];
    int nc;
    if (nt > 0) {
        nc = three_defenses(t, nt, cand);
    } else {
        // 活三ではないが、受け側がパスすれば攻め側に VCF がある (四を止めた手が脅威を残した場合など)
        if (!attacker_node(TSS_VCF, VCF_INNER_DEPTH)) return 0;
        nc = vcf_threat_cells(cand);
        if (nc < 0) return 0;
    }

    // 受け側が先に VCF で勝てるなら脅威は通らない (攻守を入れ替えて調べる)
    {
        int a = C.att;
        C.att = C.def;
        C.def = a;
        int r = attacker_node(TSS_VCF, VCF_INNER_DEPTH);
        C.def = C.att;
        C.att = a;
        if (r || C.aborted) return 0;
    }

    static uint8_t seen[NCELL];
    for (int i = 0; i < nc; i++) seen[cand[i]] = 1;
    for (int i = 0; i < nf; i++)
        if (!seen[fours[i]]) { seen[fours[i]] = 1; cand[nc++] = fours[i]; }
    for (int i = 0; i < nc; i++) {
        seen[cand[i]] = 0;
        int p = cand[i];
        score[i] = b->pat4[C.att][p] * 4 + b->pat4[C.def][p] * 3 + near_bonus(p);
    }
    sort_desc(cand, score, nc);
    for (int i = 0; i < nc; i++) {
        int p = cand[i];
        if (!legal(C.def, p)) continue;
        board_move_color(b, p, C.def);
        int r = attacker_node(mode, depth);
        board_undo(b);
        if (!r) return 0;
    }
    return 1;
}

// 勝ち手順を置換表とルールからたどる (表が上書きされていれば途中で終わる)
static int walk_line(int mode, int first, int *line, int max) {
    Board *b = C.b;
    int n = 0, played = 0;
    while (n < max - 1) {
        // 攻め側
        int f[2], q = -1;
        if (find_fives(C.att, f, 1)) { line[n++] = f[0]; break; }
        if (find_fives(C.def, f, 2) == 1) q = f[0];
        else if (n == 0) q = first;
        else {
            for (int m = mode; m >= 0 && q < 0; m--) {
                uint64_t key = b->hash ^ SALT[m][C.att];
                TTEntry *e = &tt[key & (TT_SIZE - 1)];
                if (e->key == key && e->result == TT_WIN && e->best >= 0) q = e->best;
            }
        }
        if (q < 0 || b->cell[q] != EMPTY) break;
        line[n++] = q;
        board_move_color(b, q, C.att);
        played++;
        // 受け側
        int na = find_fives(C.att, f, 2);
        if (na >= 2 || find_fives(C.def, f, 1)) break;
        if (na == 1) {
            q = f[0];
            if (!legal(C.def, q)) break;
        } else {
            int t[MAX_CAND], nt = 0, cand[MAX_MOVES];
            for (int i = 0; i < C.npos && nt < MAX_CAND; i++) {
                int p = C.pos[i];
                if (b->cell[p] == EMPTY && b->pat4[C.att][p] == P4_FLEX4 && legal(C.att, p)) t[nt++] = p;
            }
            int nc;
            if (nt > 0) {
                nc = three_defenses(t, nt, cand);
            } else {
                C.max_nodes = C.nodes + 100000;
                if (!attacker_node(TSS_VCF, VCF_INNER_DEPTH)) break;
                nc = vcf_threat_cells(cand);
            }
            q = -1;
            for (int i = 0; i < nc && q < 0; i++)
                if (legal(C.def, cand[i])) q = cand[i];
            if (q < 0) break;
        }
        line[n++] = q;
        board_move_color(b, q, C.def);
        played++;
    }
    while (played--) board_undo(b);
    return n;
}

int tss_solve(Board *b, int attacker, int mode, int max_depth, long max_nodes, TssResult *res) {
    if (!tt) {
        tt = calloc(TT_SIZE, sizeof(TTEntry));
        if (!tt) return 0;
    }
    C.b = b;
    C.att = attacker;
    C.def = opp_color(attacker);
    C.restricted = rule_has_forbidden(&b->rule);
    C.nodes = 0;
    C.max_nodes = max_nodes;
    C.aborted = 0;
    C.root_nmoves = b->nmoves;
    C.root_best = -1;
    C.npos = 0;
    for (int y = 0; y < b->size; y++)
        for (int x = 0; x < b->size; x++) C.pos[C.npos++] = to_pos(x, y);

    int win = 0;
    if (mode == TSS_VCF) {
        win = attacker_node(TSS_VCF, max_depth);
    } else {
        for (int d = 1; d <= max_depth && !win && !C.aborted; d++) win = attacker_node(TSS_VCT, d);
    }
    if (res) {
        res->win = win;
        res->move = win ? C.root_best : -1;
        res->nodes = C.nodes;
        res->aborted = C.aborted;
        res->nline = 0;
        if (win) {
            C.aborted = 0;
            res->nline = walk_line(mode, C.root_best, res->line, TSS_MAX_LINE);
        }
    }
    return win;
}

int vcf_solve(Board *b, int max_depth, long max_nodes, int *win_move) {
    TssResult r;
    int win = tss_solve(b, board_side(b), TSS_VCF, max_depth, max_nodes, &r);
    if (win_move) *win_move = r.move;
    return win;
}

int vct_solve(Board *b, int max_depth, long max_nodes, int *win_move) {
    TssResult r;
    int win = tss_solve(b, board_side(b), TSS_VCT, max_depth, max_nodes, &r);
    if (win_move) *win_move = r.move;
    return win;
}

int vcf_defenses(Board *b, long max_nodes, int *moves) {
    int me = board_side(b), op = opp_color(me);
    TssResult r;
    if (!tss_solve(b, op, TSS_VCF, 30, max_nodes, &r)) return -1;
    int restricted = rule_has_forbidden(&b->rule);
    int n = 0;
    int f[2];
    C.b = b;
    // 相手に五の点が既にあれば止めるしかない
    C.att = op;
    if (find_fives(op, f, 2)) {
        for (int i = 0; i < C.npos && n < 2; i++) {
            int p = C.pos[i];
            if (b->cell[p] == EMPTY && b->pat4[op][p] == P4_FIVE &&
                !(me == BLACK && restricted && board_is_forbidden(b, p)))
                moves[n++] = p;
        }
        return n;
    }
    // 候補: 相手の VCF を崩し得る点 (vcf_threat_cells: 手順上の点・最後の五の点・受けた点の周り) と自分の四。
    // 手順をたどれなければ盤上の空点すべて
    static uint8_t mark[NCELL];
    int cand[MAX_MOVES], nc = 0;
    C.max_nodes = C.nodes + max_nodes;
    C.aborted = 0;
    int nt = attacker_node(TSS_VCF, 30) ? vcf_threat_cells(cand) : -1;
    if (nt < 0) {
        nt = 0;
        for (int i = 0; i < C.npos; i++)
            if (b->cell[C.pos[i]] == EMPTY) cand[nt++] = C.pos[i];
    }
    for (int i = 0; i < nt; i++) {
        if (!mark[cand[i]]) { mark[cand[i]] = 1; cand[nc++] = cand[i]; }
    }
    int npos = C.npos;
    for (int i = 0; i < npos; i++) {
        int p = C.pos[i];
        if (b->cell[p] == EMPTY && b->pat4[me][p] >= P4_BLOCK4 && !mark[p]) { mark[p] = 1; cand[nc++] = p; }
    }
    long sub = max_nodes / 8 > 2000 ? max_nodes / 8 : 2000;
    for (int i = 0; i < nc; i++) {
        int p = cand[i];
        mark[p] = 0;
        if (me == BLACK && restricted && board_is_forbidden(b, p)) continue;
        if (b->pat4[me][p] >= P4_BLOCK4) { moves[n++] = p; continue; }
        TssResult v;
        board_move_color(b, p, me);
        tss_solve(b, op, TSS_VCF, 30, sub, &v);
        board_undo(b);
        if (!v.win) moves[n++] = p;    // 打ち切り (不明) も候補に残す
    }
    return n;
}
