// 五目並べ（連珠）先手は黒で(7,7)に置く
// 3種類の禁じ手のどれをルールとするかは，check変数でコンパイル時に指定する．
// 禁じ手のルールは，これらのサイトを見て作ったつもり．
//  http://iroha.poloa.net/ref/02_04.html
//  https://ja.wikipedia.org/wiki/%E4%BA%94%E7%9B%AE%E4%B8%A6%E3%81%B9

#include "gomoku.h"
#include "logics/opening_book.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int inputPutPos(int board[][BOARD_SIZE], int which, int com, int move_count);
void changeTurn(int *which_turn);
int checkOutPos(int x, int y);
int gameInit(int board[][BOARD_SIZE], int c33Board[][BOARD_SIZE],int c44Board[][BOARD_SIZE], int *which_turn, int *com);
void boardInit(int board[][BOARD_SIZE]);
void boardPrint(int board[][BOARD_SIZE]);
int gameEndProcess(int board[][BOARD_SIZE],int c33Board[][BOARD_SIZE],int c44Board[][BOARD_SIZE]);

int check = 7;  // 1:三三のみ 2:四四のみ 4:長連のみ 3:三三と四四 5:三三と長連
                // 6:四四と長連 7:全部

//=======================================================
// main
//=======================================================
int main() {
    // 変数宣言
    int board[BOARD_SIZE][BOARD_SIZE];
    int c33Board[BOARD_SIZE][BOARD_SIZE];
    int c44Board[BOARD_SIZE][BOARD_SIZE];
    int which_turn;
    int com;
    int move_count = 0;  // 手数カウンタ

    // 初期処理
    if(gameInit(board, c33Board, c44Board,  &which_turn, &com) == 1) return 1;

    boardPrint(board);

    //---- メインループ
    while(1) {
        //--- 入力処理
        if(inputPutPos(board, which_turn, com, move_count) == 1) return 1;

        //--- 手番交代処理
        changeTurn(&which_turn);
        move_count++;  // 手数を増やす

        //--- 出力処理
        boardPrint(board);

        //--- 終了判定
        if(gameEndProcess(board,c33Board,c44Board)) {
            break;
        }
    }
    return 0;
}

//-------------------------------------------------
// 置く場所入力
//-------------------------------------------------
int inputPutPos(int board[][BOARD_SIZE], int which, int com, int move_count) {
    int pos_x = -1, pos_y = -1;
    char buf[10];

    if(which == com) {
        calcPutPos(board, com, &pos_x, &pos_y);
    } else {
        printf("%s", (which == 1) ? "●" : "○");
        printf("の番です。どこに置きますか x y の順に入力してください\n> ");
        while(1) {
            if(fgets(buf, sizeof buf, stdin) == NULL || buf[0] == '\n')
                return 1;
            if(sscanf(buf, "%d %d", &pos_x, &pos_y) != 2) {
                printf("不正な入力です\n >");
                continue;
            }
            if(checkOutPos(pos_x, pos_y) &&
               board[pos_y][pos_x] == STONE_SPACE) {
                break;
            } else {
                printf("不正な入力です\n> ");
                continue;
            }
        }

        // 相手（人間）の手が定石から外れていないか検証
        if (move_count < 10) {
            validate_opponent_move(board, move_count, com, pos_x, pos_y);
        }
    }
    board[pos_y][pos_x] = which;
    return 0;
}

//-------------------------------------------------
// 手番交代処理
//-------------------------------------------------
void changeTurn(int *which_turn) {
    *which_turn = (*which_turn == STONE_BLACK) ? STONE_WHITE : STONE_BLACK;
}

//-------------------------------------------------
// 範囲外チェック
//-------------------------------------------------
int checkOutPos(int x, int y) {
    return (x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE);
}

//-------------------------------------------------
// ゲーム情報初期化
//-------------------------------------------------
int gameInit(int board[][BOARD_SIZE], int c33Board[][BOARD_SIZE], int c44Board[][BOARD_SIZE], int *which_turn, int *com) {
    int in = -1;
    char buf[256];
    do {
        printf("先手を選びますか？ Yes=>1 No=>0\n> ");
        if(fgets(buf, sizeof buf, stdin) == NULL || buf[0] == '\n') return 1;

        if(sscanf(buf, "%d", &in) != 1) {
            continue;
        }
    } while(in != 0 && in != 1);

    *com = (in == 1) ? STONE_WHITE : STONE_BLACK;

    boardInit(board);
    boardInit(c33Board);
    boardInit(c44Board);
    *which_turn = STONE_BLACK;
    return 0;
}

//-------------------------------------------------
// 盤面初期
//-------------------------------------------------
void boardInit(int board[][BOARD_SIZE]) {
    int i, j;
    for(i = 0; i < BOARD_SIZE; i++) {
        for(j = 0; j < BOARD_SIZE; j++) {
            board[i][j] = STONE_SPACE;
        }
    }
}

//-------------------------------------------------
// 盤面出力
//-------------------------------------------------
void boardPrint(int board[][BOARD_SIZE]) {
    int i, j;

    printf("    ");
    for(i = 0; i < BOARD_SIZE; i++) {
        if(i / 10)
            printf("%d ", i / 10);
        else
            printf("  ");
    }
    puts("");
    printf("    ");
    for(i = 0; i < BOARD_SIZE; i++) {
        printf("%d ", i % 10);
    }
    puts("");
    for(i = 0; i < BOARD_SIZE; i++) {
        printf("%2d ", i);
        for(j = 0; j < BOARD_SIZE; j++) {
            switch(board[i][j]) {
                case STONE_SPACE:
                    printf("・");
                    break;
                case STONE_BLACK:
                    printf("●");
                    break;
                case STONE_WHITE:
                    printf("○");
                    break;
            }
        }
        puts("");
    }
    puts("");
}
