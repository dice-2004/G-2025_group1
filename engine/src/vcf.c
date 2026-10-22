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
#define VCF_INNER_DEPTH 24      // VCT の各局面で先に試す VCF の深さ
#define MAX_CAND 128

enum { TT_NONE = 0, TT_WIN = 1, TT_FAIL = 2 };

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

    // 達四 (止められない四) が打てれば勝ち
    int cand[MAX_CAND], score[MAX_CAND], nc = 0;
    for (int i = 0; i < C.npos; i++) {
        int p = C.pos[i];
        if (b->cell[p] != EMPTY) continue;
        int p4 = b->pat4[C.att][p];
        if (p4 == P4_FLEX4) {
            if (legal(C.att, p)) return win_at(key, depth, p);
            continue;
        }
        if (p4 >= P4_BLOCK4 || (mode == TSS_VCT && is_three4(p4))) {
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
    if (nt == 0) return 0;      // 脅威になっていない

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

    int cand[MAX_MOVES], score[MAX_MOVES];
    int nc = three_defenses(t, nt, cand);
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
            if (nt == 0) break;
            int nc = three_defenses(t, nt, cand);
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
    // 候補: 相手の手順上の点、受けた点の 4 方向 ±4 (逆四の準備)、自分の四
    static uint8_t mark[NCELL];
    int cand[MAX_MOVES], nc = 0;
    for (int i = 0; i < r.nline; i++) {
        int p = r.line[i];
        if (b->cell[p] == EMPTY && !mark[p]) { mark[p] = 1; cand[nc++] = p; }
        if (i % 2 == 1) {
            for (int d = 0; d < 4; d++)
                for (int k = -4; k <= 4; k++) {
                    int q = p + k * DIR_OFFSET[d];
                    if (q >= 0 && q < NCELL && b->cell[q] == EMPTY && !mark[q]) { mark[q] = 1; cand[nc++] = q; }
                }
        }
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
