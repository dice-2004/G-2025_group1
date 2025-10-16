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
    int sum_stone = 0;
    int sum_space = 0;
    int num = 0;

    for(i = 0; i < 4; i++) {
        sum_stone = 0;
        sum_space = 0;
        if(!checkOutPos(x - dx[i], y - dy[i]) ||
           (checkOutPos(x - dx[i], y - dy[i]) &&
            board[y - dy[i]][x - dx[i]] != STONE_SPACE)) {
            continue;
        }
        for(j = 0; j < 4; j++) {
            if(checkOutPos(x + j * dx[i], y + j * dy[i])) {
                if(board[y][x] == board[y + j * dy[i]][x + j * dx[i]]) {
                    sum_stone++;
                    if(sum_stone == 3)
                        break;
                } else if(board[y + j * dy[i]][x + j * dx[i]] == STONE_SPACE) {
                    sum_space++;
                } else {
                    break;
                }
            }
        }

        if(sum_stone == 3 && sum_space == 1) {
            if(checkOutPos(x + 4 * dx[i], y + 4 * dy[i]) &&
               board[y + 4 * dy[i]][x + 4 * dx[i]] == STONE_SPACE) {
                switch(i){
                case 0: num += LEFT; break;
                case 1: num += TATE; break;
                case 2: num += YOKO; break;
                case 3: num += RIGT; break;
                }
            }
        } else if(sum_stone == 3 && sum_space == 0) {
            if(checkOutPos(x + 3 * dx[i], y + 3 * dy[i]) &&
               board[y + 3 * dy[i]][x + 3 * dx[i]] == STONE_SPACE) {
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
    int sum_stone = 0;
    int sum_space = 0;
    int num = 0;

    for(i = 0; i < 4; i++) {
        sum_stone = 0;
        sum_space = 0;
        for(j = 0; j < 5; j++) {
            if(checkOutPos(x + j * dx[i], y + j * dy[i])) {
                if(board[y][x] == board[y + j * dy[i]][x + j * dx[i]]) {
                    sum_stone++;
                    if(sum_stone == 4) break;
                } else if(board[y + j * dy[i]][x + j * dx[i]] == STONE_SPACE) {
                    sum_space++;
                } else
                    break;
            }
        }
        if(sum_stone == 4 && sum_space == 1) {
            switch(i){
            case 0: num += LEFT; break;
            case 1: num += TATE; break;
            case 2: num += YOKO; break;
            case 3: num += RIGT; break;
            }
        } else if(sum_stone == 4 && sum_space == 0) {
            if((checkOutPos(x - dx[i], y - dy[i]) &&
                board[y - dy[i]][x - dx[i]] == STONE_SPACE) ||
               (checkOutPos(x + 4 * dx[i], y + 4 * dy[i]) &&
                board[y + 4 * dy[i]][x + 4 * dx[i]] == STONE_SPACE)) {
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
    int sum_stone = 0;
    int sum_space = 0;

    for(i = 0; i < 4; i++) {
        sum_space = 0;
        sum_stone = 0;

        for(j = 0; j < 6; j++) {
            if(checkOutPos(x + j * dx[i], y + j * dy[i])) {
                if(board[y][x] == board[y + j * dy[i]][x + j * dx[i]]) {
                    sum_stone++;
                    if(sum_stone == 6) {
                        return true;
                    }
                } else if(board[y + j * dy[i]][x + j * dx[i]] == STONE_SPACE) {
                    sum_space++;
                } else
                    break;
            }
        }
    }
    return false;
}
