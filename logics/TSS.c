#include <string.h>
#include <stdio.h>
#include "TSS.h"
#include "../gomoku_ai.h"

// ========================================
// TSS: Threat Space Search（詰み探索）
// ========================================

// 特定位置に石を置いて五連になるかチェック
static int makes_five(const int board[][BOARD_SIZE], int x, int y, int color) {
    int temp_board[BOARD_SIZE][BOARD_SIZE];
    memcpy(temp_board, board, sizeof(temp_board));
    temp_board[y][x] = color;

    // 4方向をチェック
    int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

    for (int d = 0; d < 4; d++) {
        int dx = directions[d][0];
        int dy = directions[d][1];
        int count = 1;

        // 正方向をカウント
        int nx = x + dx;
        int ny = y + dy;
        while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE &&
               temp_board[ny][nx] == color) {
            count++;
            nx += dx;
            ny += dy;
        }

        // 逆方向をカウント
        nx = x - dx;
        ny = y - dy;
        while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE &&
               temp_board[ny][nx] == color) {
            count++;
            nx -= dx;
            ny -= dy;
        }

        if (count == 5) return 1;  // ちょうど5連
    }

    return 0;
}

// ========================================
// 勝ち手探索
// ========================================

// 自分が次に打って勝てる手を探す
int find_winning_move(const int board[][BOARD_SIZE], int color, int *win_x, int *win_y) {
    // 全ての空きマスをチェック
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (board[y][x] == STONE_SPACE) {
                // 禁じ手チェック（黒の場合）
                if (color == STONE_BLACK && is_forbidden(board, x, y, color)) {
                    continue;
                }

                // この手で五連ができるか
                if (makes_five(board, x, y, color)) {
                    *win_x = x;
                    *win_y = y;
                    printf("[TSS] Winning move found at (%d, %d)!\n", x, y);
                    return 1;
                }
            }
        }
    }

    return 0;
}

// ========================================
// 必死（相手の勝ち手を防ぐ）
// ========================================

// 相手が次に打って勝つ手を探す
int find_threat_move(const int board[][BOARD_SIZE], int color, int *threat_x, int *threat_y) {
    int opponent = (color == STONE_BLACK) ? STONE_WHITE : STONE_BLACK;

    // 相手の勝ち手を探す
    if (find_winning_move(board, opponent, threat_x, threat_y)) {
        printf("[TSS] Threat detected at (%d, %d), must defend!\n", *threat_x, *threat_y);
        return 1;
    }

    return 0;
}
