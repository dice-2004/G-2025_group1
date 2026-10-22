// 指し手選択 (時間の配分 → 本探索)
#ifndef G2_THINK_H
#define G2_THINK_H

#include "board.h"

typedef struct {
    int time_ms;        // 1 手の持ち時間 (Gomocup の timeout_turn)。0 = 指定なし
    int time_left_ms;   // 対局の残り時間 (time_left)。0 = 指定なし
    int max_depth;      // 0 = 制限なし (テスト用)
    long max_nodes;     // 0 = 制限なし (テスト用)
    int verbose;
} ThinkLimits;

// 手番側の着手を返す (盤上の pos)。打てる手がなければ -1
int think_move(Board *b, const ThinkLimits *lim);

#endif
