#ifndef TSS_H
#define TSS_H

#include "../gomoku.h"

// 詰みチェック（自分が次に打って勝てる手）
int find_winning_move(const int board[][BOARD_SIZE], int color, int *win_x, int *win_y);

// 必死チェック（相手が次に打って勝つ手を防ぐ）
int find_threat_move(const int board[][BOARD_SIZE], int color, int *threat_x, int *threat_y);

// 高度な詰み探索
// 戻り値: 0=見つからず, 1=開いた三, 2=開いた四
int find_strong_attack_move(const int board[][BOARD_SIZE], int color, int *attack_x, int *attack_y);
int find_strong_defense_move(const int board[][BOARD_SIZE], int color, int *defense_x, int *defense_y);

#endif
