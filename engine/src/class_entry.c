// 授業の枠組み (gomoku.c / .libgomoku.c) 用の入口。
// calcPutPos を G2 エンジンで実装し、枠組みが要求する他のシンボルはここで空実装する。
// ビルド: リポジトリ直下で `make g2class` → ./gomoku_g2
//
// 注意: 授業審判の禁じ手判定は本物の連珠と少し違う (docs/benchmarks/2026-10-22_baseline.md)。
// ここでは check 変数に合わせた連珠判定で禁じ手を避けるだけで、審判の癖までは再現していない。
#include "../../gomoku.h"
#include "board.h"
#include "think.h"
#include "vcf.h"

static Board g_board;
static int g_tables_check = -1;
static Rule g_rule;

int calcPutPos(int board[][BOARD_SIZE], int com, int *out_x, int *out_y) {
    if (g_tables_check != check) {
        if (g_tables_check < 0) board_global_init();
        g_rule = rule_from_class_check(check);
        pattern_init(&g_rule);
        tss_clear();
        g_tables_check = check;
    }
    // 手順は分からないので、黒白を交互に置いて同じ盤面を作る
    int bl[MAX_MOVES], wh[MAX_MOVES], nb = 0, nw = 0;
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (board[y][x] == STONE_BLACK) bl[nb++] = to_pos(x, y);
            else if (board[y][x] == STONE_WHITE) wh[nw++] = to_pos(x, y);
        }
    }
    board_init(&g_board, BOARD_SIZE, &g_rule);
    for (int i = 0; i < nb || i < nw; i++) {
        if (i < nb) board_move(&g_board, bl[i]);
        if (i < nw) board_move(&g_board, wh[i]);
    }
    (void)com;  // 手番は石数の偶奇で決まる (枠組みでは常に com と一致)
    ThinkLimits lim = {1000, 0, 0, 0, 0};
    int p = think_move(&g_board, &lim);
    *out_x = pos_x(p);
    *out_y = pos_y(p);
    return 0;
}

// ---- 授業の枠組みが参照するが G2 では使わないもの (学習・定石検証) ----
int get_ai_color(void) { return 0; }
void finish_game_record(int result) { (void)result; }
void start_game_record(void) {}
int validate_opponent_move(const int board[][BOARD_SIZE], int move_count, int ai_color, int opp_x, int opp_y) {
    (void)board; (void)move_count; (void)ai_color; (void)opp_x; (void)opp_y;
    return 1;
}
