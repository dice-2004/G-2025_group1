// 評価関数の表
//
// 空点 q に色 c が打ったときの 4 方向パターンの組 (pattern_index) → 点数 g_cell_value[c][idx]。
// 盤面の評価は「空点の点数の和」(Board.vsum を差分更新) の手番側 − 相手側。
// 点数は線パターンごとの重み + 4 方向の組み合わせ (Pattern4) ごとの加点で作る。
#ifndef G2_EVAL_H
#define G2_EVAL_H

#include "pattern.h"

extern int16_t g_cell_value[3][PIDX_COUNT];

// 調整できる重み (eval.c の既定値。tune 用に書き換え可能)
typedef struct {
    int line[P_COUNT];          // 線パターンごと
    int combo[P4_COUNT];        // Pattern4 ごとの加点
    int opp_scale;              // 相手側の和に掛ける割合 (/64)
    int tempo;                  // 手番の加点
} EvalParams;

extern EvalParams g_eval_params;

void eval_init(const Rule *rule);

// 点数の計算に使う Pattern4 (黒の禁じ手の補正込み)。-1 = 価値なし (黒の四四・長連の点)
int eval_cell_class(const Rule *rule, int color, const uint8_t pats[4]);   // pattern_init から呼ばれる。g_eval_params から表を作る

#endif
