#ifndef EVALUATION_H
#define EVALUATION_H

#include "../gomoku.h"
#include "../gomoku_ai.h"

// パターンの評価値定義（拡張版 - 詳細なパターン認識）
// ========================================
// 【最重要パターン】勝ち確定・防御必須
// ========================================
#define SCORE_FIVE            100000   // ●●●●● 五連（勝利確定）
#define SCORE_OPEN_FOUR       50000    // _●●●●_ 開いた四（次で勝ち確定）
#define SCORE_FOUR_FOUR       45000    // 四四（2つの四が同時）
#define SCORE_FOUR_THREE      40000    // 四三（四と三が同時）

// ========================================
// 【準勝ちパターン】強力な攻撃形
// ========================================
#define SCORE_FOUR            10000    // ●●●●_ 四連（片側ブロック、防御必須）
#define SCORE_BROKEN_FOUR     9500     // ●●_●● 飛び四（実質的な四）
#define SCORE_OPEN_THREE_THREE 9000    // 開いた三×2（勝ちパターン）
#define SCORE_OPEN_THREE      5000     // _●●●_ 開いた三（次に四を作れる）
#define SCORE_BROKEN_THREE    4500     // ●●_● または ●_●● 飛び三

// ========================================
// 【攻撃パターン】有利な形
// ========================================
#define SCORE_THREE_THREE     3500     // 三×2（強力な攻撃形）
#define SCORE_THREE           1000     // ●●●_ 三連（片側ブロック）
#define SCORE_OPEN_TWO_TWO    800      // 開いた二×2
#define SCORE_OPEN_TWO        500      // _●●_ 開いた二
#define SCORE_BROKEN_TWO      400      // ●_● 飛び二

// ========================================
// 【基本パターン】序盤・展開用
// ========================================
#define SCORE_TWO_TWO         200      // 二×2
#define SCORE_TWO             100      // ●● 二連
#define SCORE_ONE             10       // ● 一つ

// ========================================
// 【特殊パターン】位置評価
// ========================================
#define SCORE_CENTER          50       // 中央付近（戦略的に重要）
#define SCORE_NEAR_ENEMY      30       // 相手の石の近く
#define SCORE_CONNECTION      20       // 既存の石との連結

// 評価関数
int evaluate_board(const int board[][BOARD_SIZE], int my_color);
int evaluate_position(const int board[][BOARD_SIZE], int x, int y, int color);

// パターン評価（テンプレートマッチング）
int evaluate_line(const int line[], int length, int my_color);
int count_pattern(const int board[][BOARD_SIZE], int x, int y, int dx, int dy, int color);

// 深さ1の思考（単純AI）
int find_best_move_simple(const int board[][BOARD_SIZE], int color, int *best_x, int *best_y);

// パターンスコアの読み込み
int load_pattern_scores(const char *filename);
void use_default_pattern_scores(void);

// パターンスコアのグローバル変数（学習用）
extern int g_score_five;
extern int g_score_open_four;
extern int g_score_four_four;
extern int g_score_four_three;
extern int g_score_four;
extern int g_score_broken_four;
extern int g_score_open_three_three;
extern int g_score_open_three;
extern int g_score_broken_three;
extern int g_score_three_three;
extern int g_score_three;
extern int g_score_open_two_two;
extern int g_score_open_two;
extern int g_score_broken_two;
extern int g_score_two_two;
extern int g_score_two;
extern int g_score_one;
extern int g_score_center;
extern int g_score_near_enemy;
extern int g_score_connection;

#endif
