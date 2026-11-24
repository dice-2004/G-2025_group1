// 盤面: 石の配置と、各空点・各方向のパターンを差分更新で保持する
#ifndef G2_BOARD_H
#define G2_BOARD_H

#include <stdio.h>

#include "pattern.h"
#include "types.h"

// 差分更新の取り消し用: 着手で変わった (空点, 方向) の前の値
typedef struct {
    uint16_t q;
    uint8_t d, pb, pw, p4b, p4w, pad;
    uint16_t ib, iw;    // 前の pidx
} BoardUndo;
#define BOARD_UNDO_MAX ((MAX_MOVES + 16) * 40)

typedef struct {
    int32_t vsum[3];
    int16_t p4cnt[3][P4_COUNT];
    int8_t bbox[4];
} BoardSums;

#define BOARD_NLINES 64
extern uint8_t g_line_id[4][NCELL];    // [dir][pos] pos を通る方向 dir の直線の番号
extern uint8_t g_line_pos[4][NCELL];   // [dir][pos] その直線の中での位置 (+1 方向に 1 ずつ増える)

typedef struct {
    int size;
    Rule rule;
    uint8_t cell[NCELL];            // EMPTY/BLACK/WHITE/WALL
    uint64_t line[4][BOARD_NLINES]; // 方向ごとの直線の石の並び (1 マス 2 ビット、盤外は WALL)。キーはここから切り出す (board_key)
    uint8_t pat[3][NCELL][4];       // [color][pos][dir] その色がそこに打ったときの線パターン
    uint8_t pat4[3][NCELL];         // [color][pos] 4 方向合成
    uint16_t pidx[3][NCELL];        // [color][pos] pattern_index(pat[color][pos]) (差分更新で保持。表引きの添字の計算を省く)
    uint64_t hash;
    // 以下は空点についての集計 (差分更新)。探索・評価用
    int32_t vsum[3];                // [color] 空点の g_cell_value の和
    int16_t p4cnt[3][P4_COUNT];     // [color][Pattern4] その形になる空点の数
    uint8_t near[NCELL];            // 周囲 5x5 にある石の数 (候補手の範囲)
    int8_t bbox[4];                 // 石のある範囲 x0, x1, y0, y1 (石がなければ x0 = size, x1 = -1)
    int moves[MAX_MOVES];           // 着手履歴
    int nmoves;
    int passes;                     // 探索中のパス (null move) の回数の偶奇。通常は 0
    // 取り消し用の記録 (着手ごとに undo_mark から始まる)
    int nundo;
    int undo_mark[MAX_MOVES + 16];          // (禁じ手の判定で仮に置く分の余裕)
    BoardSums sums_before[MAX_MOVES + 16];
    BoardUndo undo[BOARD_UNDO_MAX];
} Board;

void board_global_init(void);                       // Zobrist 表など (1 回だけ)
void board_init(Board *b, int size, const Rule *rule);  // パターン表は呼び出し側で pattern_init 済みのこと

// 空点 pos の方向 d の周囲 10 マスの並び (中心を除く。pattern.h のキー)
static inline uint32_t line_key(uint64_t w, int lpos) {
    uint64_t t = w >> (2 * (lpos - 5));
    return (uint32_t)(t & 0x3FF) | (uint32_t)(t >> 2 & 0xFFC00);
}
static inline uint32_t board_key(const Board *b, int pos, int d) {
    return line_key(b->line[d][g_line_id[d][pos]], g_line_pos[d][pos]);
}

static inline int board_side(const Board *b) { return ((b->nmoves + b->passes) & 1) ? WHITE : BLACK; }
static inline int board_on(const Board *b, int x, int y) { return x >= 0 && y >= 0 && x < b->size && y < b->size; }

void board_move(Board *b, int pos);     // 手番側の石を置く
void board_undo(Board *b);
void board_pass(Board *b);             // 手番だけを入れ替える (もう一度呼ぶと戻る)。ハッシュに手番を混ぜる
void board_move_color(Board *b, int pos, int color);   // 手番に関係なく color を置く (board_undo で戻る)

static inline int board_pidx(const Board *b, int color, int pos) { return b->pidx[color][pos]; }

// pos に color が打ったら勝ち (五) になるか
static inline int board_is_five(const Board *b, int pos, int color) { return b->pat4[color][pos] == P4_FIVE; }

// color が pos に打った後の空点の集計 (vsum, p4cnt) を、盤を変えずに求める (葉の評価用。board_move の集計と同じ値)
void board_probe_sums(const Board *b, int pos, int color, BoardSums *out);

// 黒が pos に打つと禁じ手か (ルールに禁じ手がなければ常に 0)
int board_is_forbidden(Board *b, int pos);

// 直前の手で勝負がついたか: 戻り値 勝者 (BLACK/WHITE)、0=続行、3=引き分け(満局)
// (禁じ手の着手は呼び出し側で事前に判定する)
int board_last_move_wins(const Board *b);

// 差分ではなく盤面から作り直した key/pat が一致するか (テスト用)
int board_verify(const Board *b);

void board_print(const Board *b, FILE *fp);

// 石のある範囲を m マス広げて盤内に切った範囲 (石がなければ空の範囲)
static inline void board_area(const Board *b, int m, int *x0, int *x1, int *y0, int *y1) {
    int a = b->bbox[0] - m, c = b->bbox[1] + m, e = b->bbox[2] - m, f = b->bbox[3] + m;
    *x0 = a < 0 ? 0 : a;
    *x1 = c >= b->size ? b->size - 1 : c;
    *y0 = e < 0 ? 0 : e;
    *y1 = f >= b->size ? b->size - 1 : f;
}

#endif
