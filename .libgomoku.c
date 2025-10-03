// 五目並べ（連珠）先手は黒で(7,7)に置く
// 3種類の禁じ手のどれをルールとするかは，check変数でコンパイル時に指定する．
// 禁じ手のルールは，これらのサイトを見て作ったつもり．
//  http://iroha.poloa.net/ref/02_04.html
//  https://ja.wikipedia.org/wiki/%E4%BA%94%E7%9B%AE%E4%B8%A6%E3%81%B9

#include "gomoku.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int gameEndProcess(int board[][BOARD_SIZE],int c33Board[][BOARD_SIZE],int c44Board[][BOARD_SIZE]);
int lenCheck(int board[][BOARD_SIZE], int x, int y);
int isSan(int board[][BOARD_SIZE], int x, int y);
int isShi(int board[][BOARD_SIZE], int x, int y);
int isRoku(int board[][BOARD_SIZE], int x, int y);

//int check = 7;  // 1:三三のみ 2:四四のみ 4:長連のみ 3:三三と四四 5:三三と長連
                // 6:四四と長連 7:全部

//-------------------------------------------------
// 距離計算
//-------------------------------------------------
int distance(int x, int y, int nx, int ny) {
    int distance = -1; 
    if (x==nx) distance = abs(y-ny);
    if (y==ny) distance = abs(x-nx);
    if ((x!=nx)&&(y!=ny)&&(abs(x-nx)==abs(y-ny))) distance = abs(x-nx);
    return distance;
}

//-------------------------------------------------
// ゲーム終了判定処理
//-------------------------------------------------
int gameEndProcess(int board[][BOARD_SIZE],int c33Board[][BOARD_SIZE],int c44Board[][BOARD_SIZE]) {
    int i, j;
    int n_san = 0;
    int n_shi = 0;
    int num;

    int tmp_x =0;
    int tmp_y =0;
        
   for(i = 0; i < BOARD_SIZE; i++) {
        for(j = 0; j < BOARD_SIZE; j++) {
            if(board[i][j] == STONE_SPACE) {
                continue;
            }
            if((check & CHOLEN) && board[i][j] == STONE_BLACK &&
               (isRoku(board, j, i) == true)) {
                printf("長連の禁じ手です。%sの勝ちです\n", "○");
                return 1;
            }
        }}
    
   for(i = 0; i < BOARD_SIZE; i++) {
        for(j = 0; j < BOARD_SIZE; j++) {
            if(board[i][j] == STONE_SPACE) {
                continue;
            }
            if(lenCheck(board, j, i)) {
                printf("%sの勝ちです。\n",
                       (board[i][j] == STONE_BLACK) ? "●" : "○");
                return 1;
            }
        }}

    for(i = 0; i < BOARD_SIZE; i++) {
        for(j = 0; j < BOARD_SIZE; j++) {
            if(board[i][j] == STONE_SPACE) {
                continue;
            }
            if((check & SANSAN) && board[i][j] == STONE_BLACK &&
               (num = isSan(board, j, i))) {
                //printf("33num:%d\n", num);
                if((num>0)&&(c33Board[i][j]==0)&&(num!=LEFT)&&(num!=TATE)&&(num!=YOKO)&&(num!=RIGT)){
                    //printf("33 i=%d j=%d c33Board[i][j]=%d\n", i,j,c33Board[i][j]);
                    printf("三三の禁じ手です。%sの勝ちです\n", "○");
                    return 1;}
                if((num > 0)&&(c33Board[i][j]!=num)&&(n_san == 0)){
                    tmp_x=j;tmp_y=i;
                    n_san++;
                    c33Board[i][j]=num;
                    //printf("33 i=%d j=%d c33Board[i][j]=%d\n", i,j,c33Board[i][j]);
                };
                 if((num > 0)&&(n_san > 0)&&(c33Board[i][j]!=num)) {
                     //printf("33num-2:%d\n", num);
                     //printf("i=%d j=%d c33Board[i][j]=%d\n", i ,j, c33Board[i][j]);
                    c33Board[i][j]=num;
                    n_san++;
                    c33Board[i][j]=num;
                    if (c33Board[tmp_y][tmp_x]!=c33Board[i][j]){
                     printf("三三の禁じ手です。%sの勝ちです\n", "○");
                     return 1;}
                 }
            
            }

            if((check & SHISHI) && board[i][j] == STONE_BLACK &&
               (num = isShi(board, j, i))) {
                //printf("44num:%d\n", num);
                if((num>0)&&(c44Board[i][j]==0)&&(num!=LEFT)&&(num!=TATE)&&(num!=YOKO)&&(num!=RIGT)){
                    //printf("44num-0:%d\n", num);
                    printf("四四の禁じ手です。%sの勝ちです\n", "○");
                    return 1;}
                if((num > 0)&&(c44Board[i][j]!=num)&&(n_shi == 0)){
                    tmp_x=j;tmp_y=i;
                    n_shi++;
                    c44Board[i][j]=num;
                    //printf("44num-1:%d\n", num);
                };
                 if((num > 0)&&(n_shi > 0)&&(c44Board[i][j]!=num)) {
                    c44Board[i][j]=num;                     
                    n_shi++;
                    c44Board[i][j]=num;
                    //printf("44num-2:%d\n", num);
                    //printf("44 tmp_y=%d tmp_x=%d c44Board[tmp_y][tmp_x]=%d\n", tmp_y,tmp_x,c44Board[tmp_y][tmp_x]);
                    //printf("44 i=%d j=%d c44Board[i][j]=%d\n", i,j,c44Board[i][j]);
                    if (c44Board[tmp_y][tmp_x]!=c44Board[i][j]){
                     printf("四四の禁じ手です。%sの勝ちです\n", "○");
                     return 1;} else if (distance(tmp_y,tmp_x,i,j)>2){
                     printf("四四の禁じ手です。%sの勝ちです\n", "○");
                     return 1;}
                 }
            }
        }
    }
    
    return 0;
}

//-------------------------------------------------
// 5連確認
//-------------------------------------------------
int lenCheck(int board[][BOARD_SIZE], int x, int y) {
    int i, j, len_flag;
    int dx[] = {1, 0, 1, -1};
    int dy[] = {1, 1, 0, 1};

    // i    0:[＼]方向 1:[│]方向 2:[─]方向 3:[／]方向

    for(i = 0; i < 4; i++) {
        for(j = 0, len_flag = 1; j < 5; j++) {
            if(!checkOutPos(x + j * dx[i], y + j * dy[i]) ||
               board[y][x] != board[y + j * dy[i]][x + j * dx[i]]) {
                len_flag = 0;
                break;
            }
        }
        if(len_flag == 1) {
            return 1;
        }
    }
    return 0;
}

//-------------------------------------------------
// 三 を見つける
//-------------------------------------------------
int isSan(int board[][BOARD_SIZE], int x, int y) {
    int i, j;
    int dx[] = {1, 0, 1, -1};
    int dy[] = {1, 1, 0, 1};
    int sum_stone = 0;
    int sum_space = 0;
    int num = 0;

    // i    0:[＼]方向 1:[│]方向 2:[─]方向 3:[／]方向

    for(i = 0; i < 4; i++) {
        sum_stone = 0;
        sum_space = 0;
        // 左側が空で，４つの連続するマスの3つが石で残り１つが空で右端も空か，3連続するマスの３つが石で，右端も空
        if(!checkOutPos(x - dx[i], y - dy[i]) ||
           (checkOutPos(x - dx[i], y - dy[i]) &&
            board[y - dy[i]][x - dx[i]] != STONE_SPACE)) {
            // 左側が空でないか，盤の外
            continue;
        }
        for(j = 0; j < 4; j++) {
            // printf("%d (%d %d) %d (%d %d) \n", board[y][x], x, y, board[y + j
            // * dy[i]][x + j * dx[i]], x + j * dx[i], y + j * dy[i]);
            if(checkOutPos(x + j * dx[i], y + j * dy[i])) {
                if(board[y][x] == board[y + j * dy[i]][x + j * dx[i]]) {
                    sum_stone++;
                    if(sum_stone == 3)  // 3連続
                        break;
                } else if(board[y + j * dy[i]][x + j * dx[i]] ==
                          STONE_SPACE)  // 飛び三の可能性あり
                {
                    sum_space++;
                } else {  // 違う色の石がある
                    break;
                }
            }
        }
        // printf("%d %d sum_stone %d sum_space %d (%d %d)\n", i, j, sum_stone,
        // sum_space, board[y][x], board[y + j * dy[i]][x + j * dx[i]]);

        if(sum_stone == 3 && sum_space == 1) {
            // 右が空なら
            if(checkOutPos(x + 4 * dx[i], y + 4 * dy[i]) &&
               board[y + 4 * dy[i]][x + 4 * dx[i]] == STONE_SPACE) {
            switch(i){
            case 0: num += LEFT; break;
            case 1: num += TATE; break;
            case 2: num += YOKO; break;
            case 3: num += RIGT; break;
            }

                printf("飛び三(%d %d)\n", x, y);
                //飛び三
                //num++;
            }
        } else if(sum_stone == 3 && sum_space == 0) {
            // 右が空なら
            if(checkOutPos(x + 3 * dx[i], y + 3 * dy[i]) &&
               board[y + 3 * dy[i]][x + 3 * dx[i]] == STONE_SPACE) {
                // 3連続
            switch(i){
            case 0: num += LEFT; break;
            case 1: num += TATE; break;
            case 2: num += YOKO; break;
            case 3: num += RIGT; break;
            }
                printf("三連(%d %d)\n", x, y);
            }
        }
    }
    return num;
}

//-------------------------------------------------
// 四 を見つける
//-------------------------------------------------
int isShi(int board[][BOARD_SIZE], int x, int y) {
    int i, j;
    int dx[] = {1, 0, 1, -1};
    int dy[] = {1, 1, 0, 1};
    int sum_stone = 0;
    int sum_space = 0;
    int num = 0;

    // i    0:[＼:1]方向 1:[│:2]方向 2:[─:5]方向 3:[／:9]方向

    for(i = 0; i < 4; i++) {
        sum_stone = 0;
        sum_space = 0;
        // ５つの連続するマスの４つが石で残り１つが空か４つの連続するマスの４つが石で，両端のどちらかに石がない
        for(j = 0; j < 5; j++) {
            if(checkOutPos(x + j * dx[i], y + j * dy[i])) {
                if(board[y][x] == board[y + j * dy[i]][x + j * dx[i]]) {
                    sum_stone++;
                    if(sum_stone == 4) break;
                } else if(board[y + j * dy[i]][x + j * dx[i]] ==
                          STONE_SPACE) {  // 飛び四の可能性あり
                    sum_space++;
                } else  // 違う色の石がある
                    break;
            }
        }
        if(sum_stone == 4 && sum_space == 1) {
            // 飛び四
            switch(i){
            case 0: num += LEFT; break;
            case 1: num += TATE; break;
            case 2: num += YOKO; break;
            case 3: num += RIGT; break;
            }
            //num++;
            printf("飛び四(%d %d)\n", x, y);
        } else if(sum_stone == 4 && sum_space == 0) {
            //４連
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
                printf("四連(%d %d)\n", x, y);
            }
        }
    }
    return num;
}

//-------------------------------------------------
// ６連続を見つける
//-------------------------------------------------
int isRoku(int board[][BOARD_SIZE], int x, int y) {
    int i, j;
    int dx[] = {1, 0, 1, -1};
    int dy[] = {1, 1, 0, 1};
    int sum_stone = 0;
    int sum_space = 0;

    // i    0:[＼]方向 1:[│]方向 2:[─]方向 3:[／]方向
    for(i = 0; i < 4; i++) {
        sum_space = 0;
        sum_stone = 0;

        // 6つの連続するマスの6つが石
        for(j = 0; j < 6; j++) {
            if(checkOutPos(x + j * dx[i], y + j * dy[i])) {
                if(board[y][x] == board[y + j * dy[i]][x + j * dx[i]]) {
                    sum_stone++;
                    if(sum_stone == 6) {
                        return true;
                    }
                    /* ルール間違えてた．．．
                    if(sum_stone == 5 &&
                       sum_space == 1) {  //
                    ６つの連続するマスのうち，5個に石あり return true;
                    }
                    */
                } else if(board[y + j * dy[i]][x + j * dx[i]] ==
                          STONE_SPACE) {  // 可能性あり
                    sum_space++;
                } else  // 違う色の石がある
                    break;
            }
        }
    }
    return false;
}
