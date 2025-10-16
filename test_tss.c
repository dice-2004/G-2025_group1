// TSS機能のテストプログラム
#include <stdio.h>
#include <string.h>
#include "gomoku.h"
#include "gomoku_ai.h"
#include "logics/TSS.h"
#include "config.h"

void print_board(const int board[][BOARD_SIZE]) {
    printf("\n  ");
    for (int x = 0; x < BOARD_SIZE; x++) {
        printf("%2d", x);
    }
    printf("\n");

    for (int y = 0; y < BOARD_SIZE; y++) {
        printf("%2d ", y);
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (board[y][x] == STONE_BLACK) {
                printf("●");
            } else if (board[y][x] == STONE_WHITE) {
                printf("○");
            } else {
                printf("・");
            }
        }
        printf("\n");
    }
    printf("\n");
}

void test_open_three_horizontal() {
    printf("========================================\n");
    printf("Test 1: Horizontal Open Three\n");
    printf("========================================\n");

    int board[BOARD_SIZE][BOARD_SIZE];
    memset(board, STONE_SPACE, sizeof(board));

    // _●●_? のパターン（黒が3番目に置く）
    board[7][5] = STONE_BLACK;  // ●
    board[7][6] = STONE_BLACK;  // ●
    // board[7][7] は空き（ここに置く想定）

    printf("Initial board (expecting AI to place at (7,7) to create _●●●_):\n");
    print_board(board);

    int attack_x, attack_y;
    int result = find_strong_attack_move(board, STONE_BLACK, &attack_x, &attack_y);

    if (result == 1) {
        printf("✓ PASS: Found attack move at (%d,%d)\n", attack_x, attack_y);
        if (attack_x == 7 && attack_y == 7) {
            printf("✓ PASS: Correct position!\n");
        } else {
            printf("✗ FAIL: Wrong position (expected (7,7))\n");
        }
    } else {
        printf("✗ FAIL: No attack move found!\n");
    }
    printf("\n");
}

void test_open_three_vertical() {
    printf("========================================\n");
    printf("Test 2: Vertical Open Three\n");
    printf("========================================\n");

    int board[BOARD_SIZE][BOARD_SIZE];
    memset(board, STONE_SPACE, sizeof(board));

    // 縦に _●●_? のパターン
    board[5][7] = STONE_BLACK;
    board[6][7] = STONE_BLACK;
    // board[7][7] は空き（ここに置く想定）

    printf("Initial board (expecting AI to place at (7,7) to create vertical _●●●_):\n");
    print_board(board);

    int attack_x, attack_y;
    int result = find_strong_attack_move(board, STONE_BLACK, &attack_x, &attack_y);

    if (result == 1) {
        printf("✓ PASS: Found attack move at (%d,%d)\n", attack_x, attack_y);
        if (attack_x == 7 && attack_y == 7) {
            printf("✓ PASS: Correct position!\n");
        } else {
            printf("✗ FAIL: Wrong position (expected (7,7))\n");
        }
    } else {
        printf("✗ FAIL: No attack move found!\n");
    }
    printf("\n");
}

void test_split_three() {
    printf("========================================\n");
    printf("Test 3: Split Three Pattern\n");
    printf("========================================\n");

    int board[BOARD_SIZE][BOARD_SIZE];
    memset(board, STONE_SPACE, sizeof(board));

    // _●●_?_ のパターン（4番目に置く想定）
    board[7][5] = STONE_BLACK;
    board[7][6] = STONE_BLACK;
    // board[7][7] は空き
    board[7][8] = STONE_BLACK;
    // board[7][9] は空き（両端開き確認用）

    printf("Initial board (expecting AI to place at (7,7) to create _●●●●_):\n");
    print_board(board);

    int attack_x, attack_y;
    int result = find_strong_attack_move(board, STONE_BLACK, &attack_x, &attack_y);

    if (result >= 1) {
        printf("✓ PASS: Found attack move at (%d,%d) (level=%d)\n", attack_x, attack_y, result);
        if (attack_x == 7 && attack_y == 7) {
            printf("✓ PASS: Correct position!\n");
        } else {
            printf("✗ FAIL: Wrong position (expected (7,7))\n");
        }
    } else {
        printf("✗ FAIL: No attack move found!\n");
    }
    printf("\n");
}

void test_defense_open_three() {
    printf("========================================\n");
    printf("Test 4: Defense Against Open Three\n");
    printf("========================================\n");

    int board[BOARD_SIZE][BOARD_SIZE];
    memset(board, STONE_SPACE, sizeof(board));

    // 白（相手）が _○○_? のパターン
    board[7][5] = STONE_WHITE;
    board[7][6] = STONE_WHITE;
    // board[7][7] は空き（白が置くと _○○○_ になる）

    printf("Initial board (white has _○○_?, expecting AI to block at (7,7)):\n");
    print_board(board);

    int defense_x, defense_y;
    int result = find_strong_defense_move(board, STONE_BLACK, &defense_x, &defense_y);

    if (result == 1) {
        printf("✓ PASS: Found defense move at (%d,%d)\n", defense_x, defense_y);
        if (defense_x == 7 && defense_y == 7) {
            printf("✓ PASS: Correct blocking position!\n");
        } else {
            printf("✗ FAIL: Wrong position (expected (7,7))\n");
        }
    } else {
        printf("✗ FAIL: No defense move found!\n");
    }
    printf("\n");
}

int main() {
    printf("TSS Function Test Program\n");
    printf("========================================\n\n");

    // 設定を初期化
    load_config("config.txt");
    g_config.debug_level = 1;  // 基本的なデバッグ出力

    printf("Running TSS tests...\n\n");

    test_open_three_horizontal();
    test_open_three_vertical();
    test_split_three();
    test_defense_open_three();

    printf("========================================\n");
    printf("All tests completed!\n");
    printf("========================================\n");

    return 0;
}
