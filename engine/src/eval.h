// 評価関数の表
//
// 空点 q に色 c が打ったときの 4 方向パターンの組 (pattern_index) → 点数 g_cell_value[c][idx]。
// 盤面の評価は「空点の点数の和」(Board.vsum を差分更新) の手番側 − 相手側。
// 点数は線パターンごとの重み + 4 方向の組み合わせ (Pattern4) ごとの加点で作る。
#ifndef G2_EVAL_H
#define G2_EVAL_H

#include "pattern.h"

extern int16_t g_cell_value[3][PIDX_COUNT];

// 盤面の脅威の種類 (その色が次の 1 手で作れる形)
enum { THREAT_B4F3 = 0, THREAT_F3X2, THREAT_F3, THREAT_B4PLUS, THREAT_B4X2, THREAT_COUNT };

// 空点の Pattern4 の数 → 脅威のビット集合 (THREAT_* の位置)
static inline int eval_threat_bits(const int16_t *cnt) {
    return (cnt[P4_BLOCK4_FLEX3] > 0) << THREAT_B4F3 | (cnt[P4_FLEX3_2X] > 0) << THREAT_F3X2 |
           (cnt[P4_FLEX3] + cnt[P4_FLEX3_PLUS] > 0) << THREAT_F3 | (cnt[P4_BLOCK4_PLUS] > 0) << THREAT_B4PLUS |
           (cnt[P4_BLOCK4] + cnt[P4_BLOCK4_PLUS] + cnt[P4_BLOCK4_FLEX3] >= 2) << THREAT_B4X2;
}

// 調整できる重み (eval.c の既定値。tune 用に書き換え可能)
typedef struct {
    int line[P_COUNT];          // 線パターンごと
    int combo[P4_COUNT];        // Pattern4 ごとの加点
    int opp_scale;              // 相手側の和に掛ける割合 (/64)
    int tempo;                  // 手番の加点
    // 脅威の加点 [手番が黒=0/白=1][手番側=0/相手側=1][脅威の種類 THREAT_*]。空点の形の数 (Board.p4cnt) から O(1) で出す
    int threat[2][2][THREAT_COUNT];
} EvalParams;

extern EvalParams g_eval_params;

void eval_init(const Rule *rule);   // pattern_init から呼ばれる。g_eval_params から表を作る

// 点数の計算に使う Pattern4 (黒の禁じ手の補正込み)。-1 = 価値なし (黒の四四・長連の点)
int eval_cell_class(const Rule *rule, int color, const uint8_t pats[4]);

#endif
