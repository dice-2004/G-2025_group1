// 授業審判 (.libgomoku.c) を共有ライブラリとして bench/rules.py から呼ぶためのスタブ。
// .libgomoku.c 本体は変更せずにリンクする。
#include <stdarg.h>
#include "gomoku.h"

int check = 7;

void judge_set_check(int c) { check = c; }

int checkOutPos(int x, int y) {
    return (x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE);
}

// 学習フック: ハーネスでは何もしない (get_ai_color()==0 なら呼ばれない)
int get_ai_color(void) { return 0; }
void finish_game_record(int result) { (void)result; }
void start_game_record(void) {}

// -Dprintf=judge_quiet_printf で審判の出力を捨てる
int judge_quiet_printf(const char *fmt, ...) { (void)fmt; return 0; }
