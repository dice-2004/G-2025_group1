#include "eval.h"

int16_t g_cell_value[3][PIDX_COUNT];

EvalParams g_eval_params = {
    //        DEAD OL B1 F1 B2  F2  B3  F3  B4   F4   D4   F5
    .line = {0, 0, 1, 3, 6, 16, 20, 48, 50, 200, 200, 600},
    //         NONE F2 B3 F2x2 B3+ F3 F3+ F3x2 B4 B4+ B4F3 F4  FIVE
    .combo = {0, 0, 0, 30, 30, 0, 60, 300, 0, 50, 400, 800, 1500},
    .opp_scale = 56,
    .tempo = 20,
};

static int cell_value(const Rule *rule, int color, const uint8_t pats[4]) {
    const EvalParams *w = &g_eval_params;
    int p4 = combine_pattern4(pats);
    if (color == BLACK && rule_has_forbidden(rule) && p4 != P4_FIVE) {
        int fours = 0, f3 = 0, ol = 0;
        for (int d = 0; d < 4; d++) {
            fours += (pats[d] == P_B4 || pats[d] == P_F4) + 2 * (pats[d] == P_D4);
            f3 += pats[d] == P_F3;
            ol += pats[d] == P_OL;
        }
        // 黒の禁じ手 (長連・四四) の点は打てないので価値なし。三三は再帰判定しないと
        // 本物か分からないが、多くは禁じ手なので低く見る
        if ((rule->forbid_overline && ol) || (rule->forbid_44 && fours >= 2)) return 0;
        if (rule->forbid_33 && p4 == P4_FLEX3_2X) p4 = P4_FLEX3;
    }
    int v = w->combo[p4];
    for (int d = 0; d < 4; d++) v += w->line[pats[d]];
    return v > 32767 ? 32767 : v;
}

void eval_init(const Rule *rule) {
    for (int c = BLACK; c <= WHITE; c++) {
        for (int i = 0; i < PIDX_COUNT; i++) {
            uint8_t pats[4] = {(uint8_t)(i / (P_COUNT * P_COUNT * P_COUNT)), (uint8_t)(i / (P_COUNT * P_COUNT) % P_COUNT),
                               (uint8_t)(i / P_COUNT % P_COUNT), (uint8_t)(i % P_COUNT)};
            g_cell_value[c][i] = (int16_t)cell_value(rule, c, pats);
        }
    }
    g_cell_value[EMPTY][0] = 0;
}
