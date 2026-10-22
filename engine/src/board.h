// 盤面: 石の配置と、各空点・各方向のパターンを差分更新で保持する
#ifndef G2_BOARD_H
#define G2_BOARD_H

#include <stdio.h>

#include "pattern.h"
#include "types.h"

typedef struct {
    int size;
    Rule rule;
    uint8_t cell[NCELL];            // EMPTY/BLACK/WHITE/WALL
    uint32_t key[NCELL][4];         // 各マス・各方向の周囲 10 マスの並び
    uint8_t pat[3][NCELL][4];       // [color][pos][dir] その色がそこに打ったときの線パターン
    uint8_t pat4[3][NCELL];         // [color][pos] 4 方向合成
    uint64_t hash;
    int moves[MAX_MOVES];           // 着手履歴
    int nmoves;
} Board;

void board_global_init(void);                       // Zobrist 表など (1 回だけ)
void board_init(Board *b, int size, const Rule *rule);  // パターン表は呼び出し側で pattern_init 済みのこと

static inline int board_side(const Board *b) { return (b->nmoves % 2 == 0) ? BLACK : WHITE; }
static inline int board_on(const Board *b, int x, int y) { return x >= 0 && y >= 0 && x < b->size && y < b->size; }

void board_move(Board *b, int pos);     // 手番側の石を置く
void board_undo(Board *b);
void board_move_color(Board *b, int pos, int color);   // 手番に関係なく color を置く (board_undo で戻る)

// pos に color が打ったら勝ち (五) になるか
static inline int board_is_five(const Board *b, int pos, int color) { return b->pat4[color][pos] == P4_FIVE; }

// 黒が pos に打つと禁じ手か (ルールに禁じ手がなければ常に 0)
int board_is_forbidden(Board *b, int pos);

// 直前の手で勝負がついたか: 戻り値 勝者 (BLACK/WHITE)、0=続行、3=引き分け(満局)
// (禁じ手の着手は呼び出し側で事前に判定する)
int board_last_move_wins(const Board *b);

// 差分ではなく盤面から作り直した key/pat が一致するか (テスト用)
int board_verify(const Board *b);

void board_print(const Board *b, FILE *fp);

#endif
