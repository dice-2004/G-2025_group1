#ifndef TSS_H
#define TSS_H

#include "../gomoku.h"

// 詰みチェック（自分が次に打って勝てる手）
int find_winning_move(const int board[][BOARD_SIZE], int color, int *win_x, int *win_y);

// 必死チェック（相手が次に打って勝つ手を防ぐ）
int find_threat_move(const int board[][BOARD_SIZE], int color, int *threat_x, int *threat_y);

#endif
