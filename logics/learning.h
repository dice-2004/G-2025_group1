#ifndef LEARNING_H
#define LEARNING_H

#include "../gomoku.h"

// 学習モード
#define LEARNING_ENABLED 1

// 学習モード種別
typedef enum {
    LEARNING_MODE_PRODUCTION = 0,  // 本番モード（学習しない、データのみ参照）
    LEARNING_MODE_PRACTICE = 1     // 練習モード（学習する、データも参照）
} LearningMode;

// 対戦記録
typedef struct {
    int move_x[BOARD_SIZE * BOARD_SIZE];
    int move_y[BOARD_SIZE * BOARD_SIZE];
    int move_color[BOARD_SIZE * BOARD_SIZE];
    int move_count;
    int result;  // 1=勝ち, 0=引き分け, -1=負け
} GameRecord;

// 学習統計
typedef struct {
    int total_games;
    int wins;
    int losses;
    int draws;
    double win_rate;
} LearningStats;

// 学習機能の初期化
void init_learning(void);

// 対戦開始
void start_game_record(void);

// 手を記録
void record_move(int x, int y, int color);

// 対戦終了と学習
void finish_game_record(int result);

// 学習統計の取得
LearningStats get_learning_stats(void);

// 学習データの保存
void save_learning_data(void);

// 学習データの読み込み
void load_learning_data(void);

// 学習モードの設定・取得
void set_learning_mode(LearningMode mode);
LearningMode get_learning_mode(void);
const char* get_learning_mode_name(void);

// 現在のゲームの手数を取得
int get_current_move_count(void);

#endif
