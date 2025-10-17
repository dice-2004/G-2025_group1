/* AI思考専用版：禁じ手判定関数（ログ出力なし）// AI version

 * isSan_to, isShi_to, isRoku_to のみを提供
 * .libgomoku.cからの抜粋版（重複定義回避のため最小限の関数のみ）
 */

#include "gomoku.h"
#include <stdbool.h>

/* 三 を見つける（AI思考用：ログなし） */
int isSan_to(int board[][BOARD_SIZE], int x, int y) {
    int i, j;
    int dx[] = {1, 0, 1, -1};
    int dy[] = {1, 1, 0, 1};
    int num = 0;
    int color = board[y][x];

    for(i = 0; i < 4; i++) {
        int count = 1;  // 配置した石を含む
        int spaces = 0; // 途中の空白

        // 正方向を見る
        int right_stones = 0;
        int right_spaces = 0;
        for(j = 1; j <= 3; j++) {
            int nx = x + j * dx[i];
            int ny = y + j * dy[i];
            if(!checkOutPos(nx, ny)) break;

            if(board[ny][nx] == color) {
                right_stones++;
            } else if(board[ny][nx] == STONE_SPACE && right_stones < 2) {
                right_spaces++;
                if(right_spaces > 1) break;
            } else {
                break;
            }
        }

        // 逆方向を見る
        int left_stones = 0;
        int left_spaces = 0;
        for(j = 1; j <= 3; j++) {
            int nx = x - j * dx[i];
            int ny = y - j * dy[i];
            if(!checkOutPos(nx, ny)) break;

            if(board[ny][nx] == color) {
                left_stones++;
            } else if(board[ny][nx] == STONE_SPACE && left_stones < 2) {
                left_spaces++;
                if(left_spaces > 1) break;
            } else {
                break;
            }
        }

        count = 1 + left_stones + right_stones;
        spaces = left_spaces + right_spaces;

        // 三連判定: 石が3個で空白が1個以下、かつ両端が空いている
        if(count == 3 && spaces <= 1) {
            // 両端の空白をチェック
            int left_end_x = x - (left_stones + left_spaces + 1) * dx[i];
            int left_end_y = y - (left_stones + left_spaces + 1) * dy[i];
            int right_end_x = x + (right_stones + right_spaces + 1) * dx[i];
            int right_end_y = y + (right_stones + right_spaces + 1) * dy[i];

            int left_open = checkOutPos(left_end_x, left_end_y) &&
                           board[left_end_y][left_end_x] == STONE_SPACE;
            int right_open = checkOutPos(right_end_x, right_end_y) &&
                            board[right_end_y][right_end_x] == STONE_SPACE;

            // 両端が空いていれば開いた三
            if(left_open && right_open) {
                switch(i){
                case 0: num += LEFT; break;
                case 1: num += TATE; break;
                case 2: num += YOKO; break;
                case 3: num += RIGT; break;
                }
            }
        }
    }
    return num;
}

/* 四 を見つける（AI思考用：ログなし） */
int isShi_to(int board[][BOARD_SIZE], int x, int y) {
    int i, j;
    int dx[] = {1, 0, 1, -1};
    int dy[] = {1, 1, 0, 1};
    int num = 0;
    int color = board[y][x];

    for(i = 0; i < 4; i++) {
        int count = 1;  // 配置した石を含む
        int spaces = 0; // 途中の空白

        // 正方向を見る
        int right_stones = 0;
        int right_spaces = 0;
        for(j = 1; j <= 4; j++) {
            int nx = x + j * dx[i];
            int ny = y + j * dy[i];
            if(!checkOutPos(nx, ny)) break;

            if(board[ny][nx] == color) {
                right_stones++;
            } else if(board[ny][nx] == STONE_SPACE && right_stones < 3) {
                right_spaces++;
                if(right_spaces > 1) break;  // 空白が2つ以上は無効
            } else {
                break;
            }
        }

        // 逆方向を見る
        int left_stones = 0;
        int left_spaces = 0;
        for(j = 1; j <= 4; j++) {
            int nx = x - j * dx[i];
            int ny = y - j * dy[i];
            if(!checkOutPos(nx, ny)) break;

            if(board[ny][nx] == color) {
                left_stones++;
            } else if(board[ny][nx] == STONE_SPACE && left_stones < 3) {
                left_spaces++;
                if(left_spaces > 1) break;
            } else {
                break;
            }
        }

        count = 1 + left_stones + right_stones;
        spaces = left_spaces + right_spaces;

        // 四連判定: 石が4個で空白が1個以下
        if(count == 4 && spaces <= 1) {
            // 両端の空白をチェック
            int left_end_x = x - (left_stones + left_spaces + 1) * dx[i];
            int left_end_y = y - (left_stones + left_spaces + 1) * dy[i];
            int right_end_x = x + (right_stones + right_spaces + 1) * dx[i];
            int right_end_y = y + (right_stones + right_spaces + 1) * dy[i];

            int left_open = checkOutPos(left_end_x, left_end_y) &&
                           board[left_end_y][left_end_x] == STONE_SPACE;
            int right_open = checkOutPos(right_end_x, right_end_y) &&
                            board[right_end_y][right_end_x] == STONE_SPACE;

            // 片側以上が空いていれば四連
            if(left_open || right_open) {
                switch(i){
                case 0: num += LEFT; break;
                case 1: num += TATE; break;
                case 2: num += YOKO; break;
                case 3: num += RIGT; break;
                }
            }
        }
    }
    return num;
}

/* ６連続を見つける（AI思考用：ログなし） */
int isRoku_to(int board[][BOARD_SIZE], int x, int y) {
    int i, j;
    int dx[] = {1, 0, 1, -1};
    int dy[] = {1, 1, 0, 1};

    // 4方向をチェック
    for(i = 0; i < 4; i++) {
        int count = 1;  // 配置した石を含む

        // 正方向をカウント
        for(j = 1; j < BOARD_SIZE; j++) {
            int nx = x + j * dx[i];
            int ny = y + j * dy[i];
            if(checkOutPos(nx, ny) && board[ny][nx] == board[y][x]) {
                count++;
            } else {
                break;
            }
        }

        // 逆方向をカウント
        for(j = 1; j < BOARD_SIZE; j++) {
            int nx = x - j * dx[i];
            int ny = y - j * dy[i];
            if(checkOutPos(nx, ny) && board[ny][nx] == board[y][x]) {
                count++;
            } else {
                break;
            }
        }

        // 6連以上は長連
        if(count >= 6) {
            return true;
        }
    }
    return false;
}
