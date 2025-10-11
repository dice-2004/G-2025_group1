#ifndef OPENING_BOOK_H
#define OPENING_BOOK_H

#include "../gomoku.h"

// 序盤定石エントリ
typedef struct {
    int move_number;
    int x;
    int y;
    char comment[128];
} OpeningMove;

// 序盤定石の読み込み
int load_opening_book(const char *filename);

// 定石手の検索
int find_opening_move(const int board[][BOARD_SIZE], int move_count, int *x, int *y);

// 定石の解放
void free_opening_book(void);

#endif
