#ifndef MAIN_ENGINE_H
#define MAIN_ENGINE_H

#include "..\gomoku.h"

/* 評価関数（side 視点でのスコアを返す） */
int evaluate_board(int board[][BOARD_SIZE], int side);

#endif
