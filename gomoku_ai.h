#ifndef GOMOKU_AI_H
#define GOMOKU_AI_H

#include <stdint.h>
#include "gomoku.h"

// ビットボードのサイズ定義
#define N BOARD_SIZE
#define TOTAL (N * N)
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

// ビットボード操作関数
void setbit(uint64_t arr[], int pos);
int getbit(const uint64_t arr[], int pos);
void clearbit(uint64_t arr[], int pos);

// 盤面変換関数
void build_maps(const int board[][BOARD_SIZE], AllMaps *maps);
void maps_to_board(const AllMaps *maps, int board[][BOARD_SIZE]);

// 座標変換関数
int pos_row(int x, int y);
int pos_col(int x, int y);
int pos_diag_dr(int x, int y);
int pos_diag_dl(int x, int y);
void init_pos_tables(void);

// 禁じ手判定
int is_forbidden(const int board[][BOARD_SIZE], int x, int y, int color);
int check_sansan(const int board[][BOARD_SIZE], int x, int y);
int check_shishi(const int board[][BOARD_SIZE], int x, int y);
int check_choren(const int board[][BOARD_SIZE], int x, int y);

// 合法手生成
typedef struct {
    int x;
    int y;
} Move;

int generate_legal_moves(const int board[][BOARD_SIZE], int color, Move moves[], int max_moves);

// ビットボード上での石の配置・削除
void place_stone(AllMaps *maps, int x, int y, int color);
void remove_stone(AllMaps *maps, int x, int y, int color);

#endif
