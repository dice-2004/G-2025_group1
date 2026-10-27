#include "eval.h"

#include <string.h>

int16_t g_cell_value[3][PIDX_COUNT];

// 重みは tests/tune_eval.py で調整した値 (2026-10-22)。
// 6000 局面に Rapfi (古典評価、2 万ノード) の評価を付けて合わせた (--rapfi 6000)。自己対戦で +117 Elo
EvalParams g_eval_params = {
    //        DEAD OL B1 F1 B2 F2 B3  F3  B4   F4   D4   F5
    .line = {1, 0, 0, 0, 4, 12, 38, 0, 265, 200, 200, 600},
    //         NONE F2  B3 F2x2 B3+ F3  F3+  F3x2 B4  B4+  B4F3  F4   FIVE
    .combo = {7, 7, 0, 0, 44, 88, 197, 299, 0, 423, 2143, 800, 1500},
    .opp_scale = 49,
    .tempo = -144,
    //           黒番: 手番側 {B4F3, F3x2, F3, B4+, B4x2, B3+, F2x2, F3many}, 相手側       白番: 手番側, 相手側
    .threat = {{{576, 195, 358, -52, 11, 23, 230, 404}, {59, -46, -88, 29, -185, -69, -47, -15}},
               {{238, 432, 403, -12, 25, 141, 263, 424}, {79, 274, -189, -22, -113, -68, 37, -64}}},
    .use_table = 0,   // A/B 中 (p_table=1 で有効)
};

int eval_cell_class(const Rule *rule, int color, const uint8_t pats[4]) {
    int p4 = combine_pattern4(pats);
    if (color == BLACK && rule_has_forbidden(rule) && p4 != P4_FIVE) {
        int fours = 0, ol = 0;
        for (int d = 0; d < 4; d++) {
            fours += (pats[d] == P_B4 || pats[d] == P_F4) + 2 * (pats[d] == P_D4);
            ol += pats[d] == P_OL;
        }
        // 黒の禁じ手 (長連・四四) の点は打てないので価値なし。三三は再帰判定しないと
        // 本物か分からないが、多くは禁じ手なので活三 1 つと同じに見る
        if ((rule->forbid_overline && ol) || (rule->forbid_44 && fours >= 2)) return -1;
        if (rule->forbid_33 && p4 == P4_FLEX3_2X) p4 = P4_FLEX3;
    }
    return p4;
}

static int16_t table_delta[3][PIDX_COUNT];

static int sorted_pidx(const uint8_t pats[4]) {
    uint8_t s[4] = {pats[0], pats[1], pats[2], pats[3]};
    for (int i = 1; i < 4; i++)
        for (int j = i; j > 0 && s[j - 1] > s[j]; j--) { uint8_t t = s[j]; s[j] = s[j - 1]; s[j - 1] = t; }
    return pattern_index(s);
}

static int cell_value(const Rule *rule, int color, const uint8_t pats[4]) {
    const EvalParams *w = &g_eval_params;
    int p4 = eval_cell_class(rule, color, pats);
    if (p4 < 0) return 0;
    int v = w->combo[p4];
    for (int d = 0; d < 4; d++) v += w->line[pats[d]];
    v += table_delta[color][sorted_pidx(pats)];
    return v > 32767 ? 32767 : v < -32767 ? -32767 : v;
}

void eval_init(const Rule *rule) {
    memset(table_delta, 0, sizeof table_delta);
    if (g_eval_params.use_table && (g_eval_table_rule == 4) == (rule_has_forbidden(rule) != 0))
        for (int i = 0; i < g_eval_table_count; i++)
            table_delta[g_eval_table[i].color][g_eval_table[i].pidx] = (int16_t)g_eval_table[i].delta;
    for (int c = BLACK; c <= WHITE; c++) {
        for (int i = 0; i < PIDX_COUNT; i++) {
            uint8_t pats[4] = {(uint8_t)(i / (P_COUNT * P_COUNT * P_COUNT)), (uint8_t)(i / (P_COUNT * P_COUNT) % P_COUNT),
                               (uint8_t)(i / P_COUNT % P_COUNT), (uint8_t)(i % P_COUNT)};
            g_cell_value[c][i] = (int16_t)cell_value(rule, c, pats);
        }
    }
    g_cell_value[EMPTY][0] = 0;
}
