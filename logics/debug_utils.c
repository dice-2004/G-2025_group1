//------------------------------------------------------------------------
// デバッグユーティリティ実装
//------------------------------------------------------------------------
#include <stdio.h>
#include "debug_utils.h"
#include "../gomoku_ai.h"
#include "evaluation.h"
#include "../config.h"

// ========================================
// 盤面表示
// ========================================

void debug_print_board(const int board[][BOARD_SIZE], const char* title) {
    printf("\n========================================\n");
    printf("%s\n", title);
    printf("========================================\n");
    printf("   ");
    for (int x = 0; x < BOARD_SIZE; x++) {
        printf("%2d ", x);
    }
    printf("\n");

    for (int y = 0; y < BOARD_SIZE; y++) {
        printf("%2d ", y);
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (board[y][x] == STONE_BLACK) {
                printf(" ● ");
            } else if (board[y][x] == STONE_WHITE) {
                printf(" ○ ");
            } else {
                printf(" · ");
            }
        }
        printf("\n");
    }
    printf("========================================\n\n");
}

// ========================================
// ビットボード表示
// ========================================

void debug_print_bitmap(uint64_t bitmap, int width, int height, const char* title) {
    printf("\n[Bitmap: %s]\n", title);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int pos = y * width + x;
            printf("%d", (bitmap & (1ULL << pos)) ? 1 : 0);
        }
        printf("\n");
    }
    printf("\n");
}

// ========================================
// 探索統計表示
// ========================================

void debug_print_search_stats(int nodes, int cutoffs, int tt_hits, double time_sec) {
    printf("\n========================================\n");
    printf("Search Statistics\n");
    printf("========================================\n");
    printf("Nodes searched:     %10d\n", nodes);
    printf("Cutoffs:            %10d (%.1f%%)\n",
           cutoffs, (nodes > 0) ? (100.0 * cutoffs / nodes) : 0.0);
    printf("TT hits:            %10d (%.1f%%)\n",
           tt_hits, (nodes > 0) ? (100.0 * tt_hits / nodes) : 0.0);
    printf("Time:               %10.3f sec\n", time_sec);
    printf("Nodes/sec:          %10.0f\n",
           (time_sec > 0) ? (nodes / time_sec) : 0.0);
    printf("========================================\n\n");
}

// ========================================
// 候補手リスト表示
// ========================================

void debug_print_candidate_moves(int x[], int y[], int scores[], int count, const char* title) {
    printf("\n[%s] - %d moves\n", title, count);
    printf("----------------------------------------\n");

    int display_count = (count > 20) ? 20 : count;
    for (int i = 0; i < display_count; i++) {
        if (scores) {
            printf("%3d: (%2d, %2d) score=%d\n",
                   i + 1, x[i], y[i], scores[i]);
        } else {
            printf("%3d: (%2d, %2d)\n",
                   i + 1, x[i], y[i]);
        }
    }

    if (count > 20) {
        printf("... and %d more moves\n", count - 20);
    }
    printf("----------------------------------------\n\n");
}

// ========================================
// 評価値の詳細表示
// ========================================

void debug_print_evaluation(const int board[][BOARD_SIZE], int color) {
    if (g_config.debug_level < 2) return;

    printf("\n========================================\n");
    printf("Evaluation Details (Color=%s)\n",
           color == STONE_BLACK ? "BLACK" : "WHITE");
    printf("========================================\n");

    // 盤面を評価
    int total_score = evaluate_board(board, color);

    printf("Total Score: %d\n", total_score);

    // パターンスコア情報
    printf("\nPattern Scores:\n");
    printf("  FIVE:       %d\n", g_score_five);
    printf("  OPEN_FOUR:  %d\n", g_score_open_four);
    printf("  FOUR:       %d\n", g_score_four);
    printf("  OPEN_THREE: %d\n", g_score_open_three);
    printf("  THREE:      %d\n", g_score_three);
    printf("  OPEN_TWO:   %d\n", g_score_open_two);
    printf("  TWO:        %d\n", g_score_two);
    printf("  ONE:        %d\n", g_score_one);

    printf("========================================\n\n");
}
