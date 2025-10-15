#ifndef MAIN_ENGINE_H
#define MAIN_ENGINE_H

#include "../gomoku.h"
#include "../gomoku_ai.h"

// Alpha-Beta探索のエントリポイント
int alpha_beta_search(const int board[][BOARD_SIZE], int color, int depth,
                      int *best_x, int *best_y);

// 反復深化探索
int iterative_deepening(const int board[][BOARD_SIZE], int color, int time_limit_ms,
                        int *best_x, int *best_y);

// 早期カットオフ閾値の取得・設定（学習用）
int get_early_cutoff_threshold(void);
void set_early_cutoff_threshold(int threshold);

#endif
