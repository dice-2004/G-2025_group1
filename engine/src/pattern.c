#include "pattern.h"

#include "eval.h"

#include <string.h>

uint8_t g_pattern_table[3][KEY_COUNT];
uint8_t g_p4_table[PIDX_COUNT];

static const Rule *cur_rule;
static int cur_color;
static uint8_t *memo;  // g_pattern_table[cur_color] を兼ねる (0xFF = 未計算)

static inline int cell_at(uint32_t key, int offset) {
    return (key >> (2 * key_slot(offset))) & 3;
}

// 中心 (自石を置いた) を通る連の長さ
static int run_through_center(uint32_t key, int color) {
    int n = 1;
    for (int j = -1; j >= -5 && cell_at(key, j) == color; j--) n++;
    for (int j = 1; j <= 5 && cell_at(key, j) == color; j++) n++;
    return n;
}

static int classify(uint32_t key);

// 子 (空点 e に自石を足した並び) のパターンから親のパターン候補を作る
static int derive_from_child(int child) {
    switch (child) {
    case P_F4: return P_F3;
    case P_D4: return P_F3;   // 連珠の黒以外では D4 も止められない四
    case P_B4: return P_B3;
    case P_F3: return P_F2;
    case P_B3: return P_B2;
    case P_F2: return P_F1;
    case P_B2: return P_B1;
    default: return P_DEAD;
    }
}

static int classify(uint32_t key) {
    if (memo[key] != 0xFF) return memo[key];
    const int color = cur_color;
    const int exact = (color == BLACK) ? cur_rule->black_exact_five : cur_rule->white_exact_five;
    const int restricted = (color == BLACK) && cur_rule->forbid_overline;
    int result;

    int n = run_through_center(key, color);
    if (exact ? n == 5 : n >= 5) {
        result = P_F5;
    } else if (n >= 6 && restricted) {
        result = P_OL;
    } else {
        int five_points[10], nfp = 0;
        int best = P_DEAD;
        for (int j = -5; j <= 5; j++) {
            if (j == 0 || cell_at(key, j) != EMPTY) continue;
            uint32_t child = key | ((uint32_t)color << (2 * key_slot(j)));
            int c = classify(child);
            if (c == P_F5) {
                five_points[nfp++] = j;
            } else {
                // 連珠の黒は、達四にする手が一直線の四四や長連になるなら活三ではない
                if (restricted && c == P_D4) continue;
                int d = derive_from_child(c);
                if (d > best) best = d;
            }
        }
        if (nfp >= 2) {
            int straight = 0;
            for (int a = 0; a < nfp; a++)
                for (int b = a + 1; b < nfp; b++)
                    if (five_points[b] - five_points[a] == 5) straight = 1;
            result = straight ? P_F4 : P_D4;
        } else if (nfp == 1) {
            result = P_B4;
        } else {
            result = best;
        }
    }
    memo[key] = (uint8_t)result;
    return result;
}

void pattern_init(const Rule *rule) {
    cur_rule = rule;
    for (int color = BLACK; color <= WHITE; color++) {
        cur_color = color;
        memo = g_pattern_table[color];
        memset(memo, 0xFF, KEY_COUNT);
        for (uint32_t key = 0; key < KEY_COUNT; key++) classify(key);
    }
    for (int i = 0; i < PIDX_COUNT; i++) {
        uint8_t pats[4] = {(uint8_t)(i / (P_COUNT * P_COUNT * P_COUNT)), (uint8_t)(i / (P_COUNT * P_COUNT) % P_COUNT),
                           (uint8_t)(i / P_COUNT % P_COUNT), (uint8_t)(i % P_COUNT)};
        g_p4_table[i] = (uint8_t)combine_pattern4(pats);
    }
    eval_init(rule);   // 評価の表もルールごとに作り直す
}

int combine_pattern4(const uint8_t pats[4]) {
    int cnt[P_COUNT] = {0};
    for (int i = 0; i < 4; i++) cnt[pats[i]]++;
    if (cnt[P_F5]) return P4_FIVE;
    int fours = cnt[P_B4];
    if (cnt[P_F4] || cnt[P_D4] || fours >= 2) return P4_FLEX4;
    int threes_or_twos = cnt[P_B3] + cnt[P_F2];
    if (fours) {
        if (cnt[P_F3]) return P4_BLOCK4_FLEX3;
        if (threes_or_twos) return P4_BLOCK4_PLUS;
        return P4_BLOCK4;
    }
    if (cnt[P_F3] >= 2) return P4_FLEX3_2X;
    if (cnt[P_F3]) return threes_or_twos ? P4_FLEX3_PLUS : P4_FLEX3;
    if (cnt[P_B3] && (cnt[P_B3] + cnt[P_F2]) >= 2) return P4_BLOCK3_PLUS;
    if (cnt[P_F2] >= 2) return P4_FLEX2_2X;
    if (cnt[P_B3]) return P4_BLOCK3;
    if (cnt[P_F2]) return P4_FLEX2;
    return P4_NONE;
}

const char *pattern_name(int p) {
    static const char *names[P_COUNT] = {"DEAD", "OL", "B1", "F1", "B2", "F2", "B3", "F3", "B4", "F4", "D4", "F5"};
    return (p >= 0 && p < P_COUNT) ? names[p] : "?";
}

const char *pattern4_name(int p4) {
    static const char *names[P4_COUNT] = {
        "NONE", "FLEX2", "BLOCK3", "FLEX2_2X", "BLOCK3_PLUS", "FLEX3", "FLEX3_PLUS",
        "FLEX3_2X", "BLOCK4", "BLOCK4_PLUS", "BLOCK4_FLEX3", "FLEX4", "FIVE"};
    return (p4 >= 0 && p4 < P4_COUNT) ? names[p4] : "?";
}
