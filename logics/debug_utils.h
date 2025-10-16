//------------------------------------------------------------------------
// デバッグユーティリティ: 盤面表示、統計情報など
//------------------------------------------------------------------------
#ifndef DEBUG_UTILS_H
#define DEBUG_UTILS_H

#include <stdint.h>
#include "../gomoku.h"
#include "../gomoku_ai.h"

// 盤面を視覚的に表示
void debug_print_board(const int board[][BOARD_SIZE], const char* title);

// ビットボードを表示
void debug_print_bitmap(uint64_t bitmap, int width, int height, const char* title);

// 探索統計を表示
void debug_print_search_stats(int nodes, int cutoffs, int tt_hits, double time_sec);

// 候補手リストを表示 (Moveはgomoku_ai.hで定義)
void debug_print_candidate_moves(int x[], int y[], int scores[], int count, const char* title);

// 評価値の詳細を表示
void debug_print_evaluation(const int board[][BOARD_SIZE], int color);

#endif
