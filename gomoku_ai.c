//------------------------------------------------------------------------
// 五目ならべAI
//  ここを作る．
//  下記のサンプルは，直前に自分が置いた石の8近傍のどこかにランダムに石を置くだけ
//
//  引数説明
//    board[BOARD_SIZE][BORARD_SIZE]：五目並べの盤
//    com ： コンピュータが白石(STONE_WHITE)か黒石(STONE_BLACK)かを表す．
//    pos_x, pos_y：石を置く場所のx座標，y座標 両方出力用
//------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include "gomoku.h"
#include "logics/main_engine.h"
#include "logics/TSS.h"

int calcPutPos(int board[][BOARD_SIZE], int com, int *pos_x, int *pos_y){
    static int pre[2] = {
        // 直前の石の場所を覚えておくための変数 適当に初期化
        BOARD_SIZE / 2,
        BOARD_SIZE / 2,
    };
    static int flag = 0;
    int count = 0;

     /* 配列構成:
         - board を直列化して pos=0..TOTAL-1 に割り当て
         - pos=0 が最初の uint64_t の MSB (bit 63) に対応
         - 各向き(row/col/diag_dr/diag_dl) は Orientation 構造体で保持
         - Orientation は uint64_t black[WORDS], white[WORDS], occ[WORDS]
     */
    {
        enum { N = BOARD_SIZE };
        enum { TOTAL = N * N };
        enum { WORDS = (TOTAL + 63) / 64 };

        typedef struct {
            uint64_t black[WORDS];
            uint64_t white[WORDS];
            uint64_t occ[WORDS];
        } Orientation;

        static struct {
            Orientation row;
            Orientation col;
            Orientation diag_dr;
            Orientation diag_dl;
        } maps;

        /* 0 クリア */
        memset(&maps, 0, sizeof(maps));

        /* ビットをセットするマクロ: pos は 0..TOTAL-1 の直列位置。pos=0 が最初の単語の MSB に対応 */
        #define SETBIT(arr, pos) do { size_t __wi = (pos) / 64; size_t __bi = (pos) % 64; arr[__wi] |= (uint64_t)1ULL << (63 - __bi); } while(0)

        int p = 0;
        /* row-major: board[y][x] の順に直列化（y=0,x=0 が pos=0） */
        p = 0;
        for (int y = 0; y < N; ++y) {
            for (int x = 0; x < N; ++x) {
                int v = board[y][x];
                if (v == STONE_BLACK) SETBIT(maps.row.black, p);
                if (v == STONE_WHITE) SETBIT(maps.row.white, p);
                if (v != STONE_SPACE) SETBIT(maps.row.occ, p);
                ++p;
            }
        }

        /* column-major: x major */
        p = 0;
        for (int x = 0; x < N; ++x) {
            for (int y = 0; y < N; ++y) {
                int v = board[y][x];
                if (v == STONE_BLACK) SETBIT(maps.col.black, p);
                if (v == STONE_WHITE) SETBIT(maps.col.white, p);
                if (v != STONE_SPACE) SETBIT(maps.col.occ, p);
                ++p;
            }
        }

        /* diag down-right: x - y = k, k=-(N-1)..(N-1) */
        p = 0;
        for (int k = -(N - 1); k <= (N - 1); ++k) {
            for (int y = 0; y < N; ++y) {
                int x = y + k;
                if (x >= 0 && x < N) {
                    int v = board[y][x];
                    if (v == STONE_BLACK) SETBIT(maps.diag_dr.black, p);
                    if (v == STONE_WHITE) SETBIT(maps.diag_dr.white, p);
                    if (v != STONE_SPACE) SETBIT(maps.diag_dr.occ, p);
                    ++p;
                }
            }
        }

        /* diag down-left: x + y = k, k=0..2*(N-1) */
        p = 0;
        for (int k = 0; k <= 2 * (N - 1); ++k) {
            int y0 = (k - (N - 1) > 0) ? (k - (N - 1)) : 0;
            int y1 = (k < (N - 1)) ? k : (N - 1);
            for (int y = y0; y <= y1; ++y) {
                int x = k - y;
                if (x >= 0 && x < N) {
                    int v = board[y][x];
                    if (v == STONE_BLACK) SETBIT(maps.diag_dl.black, p);
                    if (v == STONE_WHITE) SETBIT(maps.diag_dl.white, p);
                    if (v != STONE_SPACE) SETBIT(maps.diag_dl.occ, p);
                    ++p;
                }
            }
        }

        /* 出力用バッファ（static） */
        static char outbuf[TOTAL + 1];
        outbuf[TOTAL] = '\0';

        /* ヘルパー: uint64_t 配列から pos を取り出す */
        #define GETBIT(arr, pos) ( ( (arr)[(pos) / 64] >> (63 - ((pos) % 64)) ) & 1ULL )

        /* row */
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = GETBIT(maps.row.black, i) ? '1' : '0';
        printf("[Bitmap row-major] black=%s\n", outbuf);
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = GETBIT(maps.row.white, i) ? '1' : '0';
        printf("[Bitmap row-major] white=%s\n", outbuf);
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = GETBIT(maps.row.occ, i) ? '1' : '0';
        printf("[Bitmap row-major] occ  =%s\n", outbuf);

        /* col */
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = GETBIT(maps.col.black, i) ? '1' : '0';
        printf("[Bitmap col-major] black=%s\n", outbuf);
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = GETBIT(maps.col.white, i) ? '1' : '0';
        printf("[Bitmap col-major] white=%s\n", outbuf);
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = GETBIT(maps.col.occ, i) ? '1' : '0';
        printf("[Bitmap col-major] occ  =%s\n", outbuf);

        /* diag DR */
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = GETBIT(maps.diag_dr.black, i) ? '1' : '0';
        printf("[Bitmap diag-DR   ] black=%s\n", outbuf);
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = GETBIT(maps.diag_dr.white, i) ? '1' : '0';
        printf("[Bitmap diag-DR   ] white=%s\n", outbuf);
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = GETBIT(maps.diag_dr.occ, i) ? '1' : '0';
        printf("[Bitmap diag-DR   ] occ  =%s\n", outbuf);

        /* diag DL */
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = GETBIT(maps.diag_dl.black, i) ? '1' : '0';
        printf("[Bitmap diag-DL   ] black=%s\n", outbuf);
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = GETBIT(maps.diag_dl.white, i) ? '1' : '0';
        printf("[Bitmap diag-DL   ] white=%s\n", outbuf);
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = GETBIT(maps.diag_dl.occ, i) ? '1' : '0';
        printf("[Bitmap diag-DL   ] occ  =%s\n", outbuf);

        /* マクロの後始末 */
        #undef SETBIT
        #undef GETBIT
    }

    // コンピュータが最初の1手の場合は，(7, 7)に石を置く
    if (flag == 0)
    {
        srand((unsigned int)time(NULL));
        flag = 1;
        if (com == STONE_BLACK)
        {
            *pos_y = BOARD_SIZE / 2;
            *pos_x = BOARD_SIZE / 2;
            return 0;
        }
    }
    while (1)
    {
        //乱数で前に置いた場所の8近傍に置く
        if (count > 5)
        { // 5回置こうとして，どこにも置けなくなった場合はどこでもよいからおいてみる
            *pos_y = (int)(rand() % 15);
            *pos_x = (int)(rand() % 15);
            printf("%d\n", count);
        }
        else
        {
            *pos_y = pre[0] + (int)(rand() % 3 - 1);
            *pos_x = pre[1] + (int)(rand() % 3 - 1);
        }
        if (checkOutPos(*pos_x, *pos_y))
        {
            if (board[*pos_y][*pos_x] == STONE_SPACE)
            {
                break;
            }
            count++;
        }
        else
        { // bug出し
            printf("out of board(%d %d)\n", *pos_x, *pos_y);
            count++;
        }
    }
    pre[0] = *pos_y;
    pre[1] = *pos_x;

    return 0;
}
