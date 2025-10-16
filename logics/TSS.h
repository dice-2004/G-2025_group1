#ifndef TSS_H
#define TSS_H

#include "../gomoku.h"

// 詰みチェック（自分が次に打って勝てる手）
int find_winning_move(const int board[][BOARD_SIZE], int color, int *win_x, int *win_y);

// 必死チェック（相手が次に打って勝つ手を防ぐ）
int find_threat_move(const int board[][BOARD_SIZE], int color, int *threat_x, int *threat_y);

// 高度な詰み探索
// 戻り値: 0=見つからず, 1=片側空き四, 2=開いた四, 3=開いた三
int find_strong_attack_move(const int board[][BOARD_SIZE], int color, int *attack_x, int *attack_y);
int find_strong_defense_move(const int board[][BOARD_SIZE], int color, int *defense_x, int *defense_y);

// 四連防御（開いた四と片側空き四の両方を検出）
int find_four_defense_move(const int board[][BOARD_SIZE], int color, int *defense_x, int *defense_y);

// 攻撃的な手を探す（四連または複数の開いた三を作る）
// 戻り値: 0=なし, 1=四連, 2=複数の開いた三
int find_aggressive_attack_move(const int board[][BOARD_SIZE], int color, int *attack_x, int *attack_y);

#endif
