#ifndef OPENING_BOOK_H
#define OPENING_BOOK_H

#include "../gomoku.h"

// 序盤定石エントリ（両者の手を含む）
typedef struct {
    int game_id;        // ゲーム番号（101-999）
    int move_number;    // ゲーム全体での手数（1-10）
    int player_color;   // この手を打つプレイヤー（STONE_BLACK or STONE_WHITE）
    int x;
    int y;
    double win_rate;    // 勝率 (0.0～1.0)
    char comment[128];
} OpeningMove;

// 序盤定石の読み込み
int load_opening_book(const char *filename);

// 定石手の検索（AIの手番時）
int find_opening_move(const int board[][BOARD_SIZE], int move_count, int ai_color, int *x, int *y);

// 相手の応手が定石通りか検証（相手の手番後に呼ぶ）
int validate_opponent_move(const int board[][BOARD_SIZE], int move_count, int ai_color, int opp_x, int opp_y);

// 定石の解放
void free_opening_book(void);

// 定石の勝率を更新（学習用）
int update_opening_win_rate(int game_id, int win);

// 定石の勝率をCSVに保存
int save_opening_book(const char *filename);

// 現在選択されているゲームIDを取得
int get_selected_game_id(void);

#endif
