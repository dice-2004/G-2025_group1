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

// ビットマップのサイズ定義
#define N BOARD_SIZE
// 最大ビットマップインデント
#define TOTAL (N * N)
// 64bit*要素数
#define WORDS ((TOTAL + 63) / 64)

// ビットマップ構造体
typedef struct {
    uint64_t black[WORDS];
    uint64_t white[WORDS];
    uint64_t fill[WORDS];
} Orientation;

// 四方向ビットマップ
typedef struct {
    Orientation row;
    Orientation col;
    Orientation diag_dr;
    Orientation diag_dl;
} AllMaps;

// 斜めビットテーブルとインデックスの変換表
static int pos_dr[N][N];
static int pos_dl[N][N];

// 斜めビットテーブル初期化フラグ　実初期化:0, 初期化済:1
static int pos_dr_table_is_init = 0;
static int pos_dl_table_is_init = 0;

// プロトタイプ宣言
static inline void setbit(uint64_t arr[], int pos);
static inline int getbit(const uint64_t arr[], int pos);
static inline int pos_row(int x, int y);
static inline int pos_col(int x, int y);
static void init_pos_tables(void);
static inline void build_maps(const int board[][BOARD_SIZE], AllMaps *maps);


int calcPutPos(int board[][BOARD_SIZE], int com, int *pos_x, int *pos_y){
    static int pre[2] = {
        // 直前の石の場所を覚えておくための変数 適当に初期化
        BOARD_SIZE / 2,
        BOARD_SIZE / 2,
    };
    static int flag = 0;
    int count = 0;

     /* 配列構成:
         - board を直列化したビットマップを4方向分保持
         - 各ビットマップは uint64_t 配列で保持
         - 配列の要素数は (TOTAL+63)/64 個
         - ビットマップ変換表（保存版は pos.txt 参照）
     */
    // 変数スコープ限定
    {
        AllMaps maps_local;
        build_maps(board, &maps_local);

        // ビットマップ表示（全方向）
#define DEBUG_BITMAPS 1
        static char outbuf[TOTAL + 1];
        outbuf[TOTAL] = '\0';
        // row-major
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = getbit(maps_local.row.black, i) ? '1' : '0';
        printf("[Bitmap row-major] black=%s\n", outbuf);
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = getbit(maps_local.row.white, i) ? '1' : '0';
        printf("[Bitmap row-major] white=%s\n", outbuf);
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = getbit(maps_local.row.fill, i) ? '1' : '0';
        printf("[Bitmap row-major] fill =%s\n", outbuf);
        // col-major
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = getbit(maps_local.col.black, i) ? '1' : '0';
        printf("[Bitmap col-major] black=%s\n", outbuf);
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = getbit(maps_local.col.white, i) ? '1' : '0';
        printf("[Bitmap col-major] white=%s\n", outbuf);
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = getbit(maps_local.col.fill, i) ? '1' : '0';
        printf("[Bitmap col-major] fill =%s\n", outbuf);
        // diag down-right
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = getbit(maps_local.diag_dr.black, i) ? '1' : '0';
        printf("[Bitmap diag-DR  ] black=%s\n", outbuf);
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = getbit(maps_local.diag_dr.white, i) ? '1' : '0';
        printf("[Bitmap diag-DR  ] white=%s\n", outbuf);
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = getbit(maps_local.diag_dr.fill, i) ? '1' : '0';
        printf("[Bitmap diag-DR  ] fill =%s\n", outbuf);
        // diag down-left
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = getbit(maps_local.diag_dl.black, i) ? '1' : '0';
        printf("[Bitmap diag-DL  ] black=%s\n", outbuf);
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = getbit(maps_local.diag_dl.white, i) ? '1' : '0';
        printf("[Bitmap diag-DL  ] white=%s\n", outbuf);
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = getbit(maps_local.diag_dl.fill, i) ? '1' : '0';
        printf("[Bitmap diag-DL  ] fill =%s\n", outbuf);
        /* build_maps を呼べば各テンプレートマッチや評価ルーチンで maps_local を参照してください */
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

// ビットマップにビットセット
static inline void setbit(uint64_t arr[], int pos) {
    size_t wi = pos / 64;
    size_t bi = pos % 64;
    arr[wi] |= (uint64_t)1ULL << (63 - bi);
}

// ビットマップからビット取得
static inline int getbit(const uint64_t arr[], int pos) {
    return (int)((arr[pos / 64] >> (63 - (pos % 64))) & 1ULL);
}

// 縦横のインデックス変換ヘルパー関数
static inline int pos_row(int x, int y) { return y * N + x; }
static inline int pos_col(int x, int y) { return x * N + y; }

// 斜めビットテーブルto インデックス変換表初期化ルーチン
static void init_pos_tables(void) {
    if (pos_dr_table_is_init && pos_dl_table_is_init) return;
    int p = 0;
    for (int k = -(N - 1); k <= (N - 1); ++k) {
        for (int y = 0; y < N; ++y) {
            int x = y + k;
            if (x >= 0 && x < N) {
                pos_dr[x][y] = p;
                ++p;
            }
        }
    }
    pos_dr_table_is_init = 1;

    p = 0;
    for (int k = 0; k <= 2 * (N - 1); ++k) {
        // 参考三項演算子
        int y0 = (k - (N - 1) > 0) ? (k - (N - 1)) : 0;
        int y1 = (k < (N - 1)) ? k : (N - 1);
        for (int y = y0; y <= y1; ++y) {
            int x = k - y;
            if (x >= 0 && x < N) {
                pos_dl[x][y] = p;
                ++p;
            }
        }
    }
    pos_dl_table_is_init = 1;
}
// ファイル内のみ呼び出し可能なビットマップ生成ルーチン
static inline void build_maps(const int board[][BOARD_SIZE], AllMaps *maps) {

    memset(maps, 0, sizeof(AllMaps));
    init_pos_tables();

    // 横
    int p;
    p = 0;
    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            int v = board[y][x];
            if (v == STONE_BLACK) setbit(maps->row.black, p);
            if (v == STONE_WHITE) setbit(maps->row.white, p);
            if (v != STONE_SPACE) setbit(maps->row.fill, p);
            ++p;
        }
    }

    // 縦
    p = 0;
    for (int x = 0; x < N; ++x) {
        for (int y = 0; y < N; ++y) {
            int v = board[y][x];
            if (v == STONE_BLACK) setbit(maps->col.black, p);
            if (v == STONE_WHITE) setbit(maps->col.white, p);
            if (v != STONE_SPACE) setbit(maps->col.fill, p);
            ++p;
        }
    }

    // 右斜め
    p = 0;
    for (int k = -(N - 1); k <= (N - 1); ++k) {
        for (int y = 0; y < N; ++y) {
            int x = y + k;
            if (x >= 0 && x < N) {
                int v = board[y][x];
                if (v == STONE_BLACK) setbit(maps->diag_dr.black, p);
                if (v == STONE_WHITE) setbit(maps->diag_dr.white, p);
                if (v != STONE_SPACE) setbit(maps->diag_dr.fill, p);
                ++p;
            }
        }
    }

    // 左斜め
    p = 0;
    for (int k = 0; k <= 2 * (N - 1); ++k) {
        int y0 = (k - (N - 1) > 0) ? (k - (N - 1)) : 0;
        int y1 = (k < (N - 1)) ? k : (N - 1);
        for (int y = y0; y <= y1; ++y) {
            int x = k - y;
            if (x >= 0 && x < N) {
                int v = board[y][x];
                if (v == STONE_BLACK) setbit(maps->diag_dl.black, p);
                if (v == STONE_WHITE) setbit(maps->diag_dl.white, p);
                if (v != STONE_SPACE) setbit(maps->diag_dl.fill, p);
                ++p;
            }
        }
    }
}
