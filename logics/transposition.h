#ifndef TRANSPOSITION_H
#define TRANSPOSITION_H

#include <stdint.h>
#include "../gomoku.h"

// エントリタイプ
#define TT_EXACT 0   // 正確な値
#define TT_ALPHA 1   // α値（下限）
#define TT_BETA  2   // β値（上限）

// 置換表エントリ
typedef struct {
    uint64_t hash;      // Zobristハッシュ値
    int score;          // 評価値
    int depth;          // 探索深度
    int type;           // エントリタイプ
    int best_x;         // 最善手x座標
    int best_y;         // 最善手y座標
} TTEntry;

// 置換表
typedef struct {
    TTEntry *entries;
    int size;
    uint64_t hits;
    uint64_t queries;
} TranspositionTable;

// Zobrist Hashing用の乱数テーブル
extern uint64_t zobrist_table[BOARD_SIZE][BOARD_SIZE][3];

// 初期化
void init_zobrist(void);
void init_transposition_table(int size);
void free_transposition_table(void);

// Zobristハッシュ計算
uint64_t compute_zobrist_hash(const int board[][BOARD_SIZE]);
uint64_t update_zobrist_hash(uint64_t hash, int x, int y, int stone);

// 置換表操作
int probe_tt(uint64_t hash, int depth, int alpha, int beta, int *score, int *best_x, int *best_y);
void store_tt(uint64_t hash, int depth, int score, int type, int best_x, int best_y);
void clear_tt(void);
void print_tt_stats(void);

#endif
