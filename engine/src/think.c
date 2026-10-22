// 暫定の指し手選択: 強制手 + 1 手読みのパターン点。
// Phase 2 で PVS 探索と VCF/VCT に置き換える。
#include "think.h"

#include <stdlib.h>

static const int ATTACK[P4_COUNT] = {
    0, 20, 15, 60, 80, 300, 600, 3000, 200, 800, 6000, 20000, 1000000};
static const int DEFENSE[P4_COUNT] = {
    0, 15, 10, 40, 50, 200, 400, 1500, 100, 400, 3000, 10000, 100000};
static const int LINE_VALUE[P_COUNT] = {0, 0, 1, 2, 4, 8, 6, 12, 10, 20, 20, 50};

// 石の周り 2 マス以内の空点を候補にする
static int gen_candidates(const Board *b, int *out) {
    static uint8_t mark[NCELL];
    int n = 0;
    if (b->nmoves == 0) {
        out[n++] = to_pos(b->size / 2, b->size / 2);
        return n;
    }
    for (int i = 0; i < b->nmoves; i++) {
        int p = b->moves[i];
        for (int dy = -2; dy <= 2; dy++) {
            for (int dx = -2; dx <= 2; dx++) {
                int q = p + dy * STRIDE + dx;
                if (b->cell[q] == EMPTY && !mark[q]) {
                    mark[q] = 1;
                    out[n++] = q;
                }
            }
        }
    }
    for (int i = 0; i < n; i++) mark[out[i]] = 0;
    return n;
}

static int line_bonus(const Board *b, int p, int color) {
    int s = 0;
    for (int d = 0; d < 4; d++) s += LINE_VALUE[b->pat[color][p][d]];
    return s;
}

int think_move(Board *b, const ThinkLimits *lim) {
    (void)lim;
    int cand[MAX_MOVES];
    int n = gen_candidates(b, cand);
    int me = board_side(b), op = opp_color(me);
    int restricted = (me == BLACK) && rule_has_forbidden(&b->rule);

    // 禁じ手を除く
    int legal[MAX_MOVES], nl = 0;
    for (int i = 0; i < n; i++)
        if (!(restricted && board_is_forbidden(b, cand[i]))) legal[nl++] = cand[i];
    if (nl == 0) {
        // 全部禁じ手か候補なし: 盤上の空点ならどこでも
        for (int y = 0; y < b->size; y++)
            for (int x = 0; x < b->size; x++)
                if (b->cell[to_pos(x, y)] == EMPTY) return to_pos(x, y);
        return -1;
    }

    // 1. 自分の五
    for (int i = 0; i < nl; i++)
        if (b->pat4[me][legal[i]] == P4_FIVE) return legal[i];
    // 2. 相手の五を止める
    for (int i = 0; i < nl; i++)
        if (b->pat4[op][legal[i]] == P4_FIVE) return legal[i];
    // 3. 自分の達四 (相手に五の脅威がないので勝ち)
    for (int i = 0; i < nl; i++)
        if (b->pat4[me][legal[i]] == P4_FLEX4) return legal[i];

    // 4. 相手に達四の脅威 (活三) があれば、止める手か自分の四だけを候補にする
    int opp_threat = 0;
    for (int i = 0; i < n; i++)
        if (b->pat4[op][cand[i]] == P4_FLEX4) opp_threat = 1;

    int best = -1;
    long best_score = -1;
    for (int i = 0; i < nl; i++) {
        int p = legal[i];
        int a = b->pat4[me][p], d = b->pat4[op][p];
        if (opp_threat && a < P4_BLOCK4 && d < P4_FLEX4) {
            // 防ぎ手でも四でもない手は、相手の活三を止める位置 (相手の達四点の隣) でなければ除く
            int blocks = 0;
            for (int dir = 0; dir < 4 && !blocks; dir++) {
                int pt = b->pat[op][p][dir];
                if (pt == P_B4 || pt == P_F3) blocks = 1;
            }
            if (!blocks) continue;
        }
        long s = (long)ATTACK[a] + DEFENSE[d] + line_bonus(b, p, me) + line_bonus(b, p, op) / 2;
        s = s * 16 + (rand() & 15);
        if (s > best_score) {
            best_score = s;
            best = p;
        }
    }
    if (best < 0) best = legal[0];
    return best;
}
