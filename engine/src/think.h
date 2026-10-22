// 指し手選択
#ifndef G2_THINK_H
#define G2_THINK_H

#include "board.h"

typedef struct {
    int time_ms;        // 1 手の目安 (今は未使用。探索導入時に使う)
    int verbose;
} ThinkLimits;

// 手番側の着手を返す (盤上の pos)。打てる手がなければ -1
int think_move(Board *b, const ThinkLimits *lim);

#endif
