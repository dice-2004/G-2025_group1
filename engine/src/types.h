// G2 エンジン 共通定義
#ifndef G2_TYPES_H
#define G2_TYPES_H

#include <stdint.h>

#define MAX_SIZE 20                     // 盤の最大サイズ (Gomocup の 20 路まで)
#define PAD 5                           // 盤外の番兵の幅 (パターン窓が中心から ±5 マス)
#define STRIDE (MAX_SIZE + 2 * PAD)     // 1 次元配列の行の長さ
#define NCELL (STRIDE * STRIDE)
#define MAX_MOVES (MAX_SIZE * MAX_SIZE)

enum { EMPTY = 0, BLACK = 1, WHITE = 2, WALL = 3 };

static inline int opp_color(int c) { return 3 - c; }

// 盤上の位置 (x, y) と 1 次元インデックスの変換
static inline int to_pos(int x, int y) { return (y + PAD) * STRIDE + (x + PAD); }
static inline int pos_x(int p) { return p % STRIDE - PAD; }
static inline int pos_y(int p) { return p / STRIDE - PAD; }

// 4 方向: 横, 縦, 右下, 左下
static const int DIR_OFFSET[4] = { 1, STRIDE, STRIDE + 1, STRIDE - 1 };

// ルール。Gomocup の rule 番号 (0/1/4) と授業審判の check 変数 (1..7) から作る
typedef struct {
    int black_exact_five;   // 黒は「ちょうど五」でなければ勝ちにならない
    int white_exact_five;   // 白は「ちょうど五」でなければ勝ちにならない
    int forbid_overline;    // 黒の長連は禁じ手 (負け)
    int forbid_44;          // 黒の四四は禁じ手
    int forbid_33;          // 黒の三三は禁じ手
    int gomocup_rule;       // 元の Gomocup ルール番号 (表示用)
} Rule;

Rule rule_from_gomocup(int r);   // 0=freestyle, 1=standard, 4=renju
Rule rule_from_class_check(int check);  // 授業審判の check (1..7)
int rule_has_forbidden(const Rule *r);

#endif
