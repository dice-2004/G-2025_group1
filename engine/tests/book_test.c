// g2-book: 開局ブックの表引きを試す。標準入力の 1 行 = "<rule> x,y x,y ..." (黒から交互の着手列)。
// 出力の 1 行 = 引いた手 "x,y" (ミスなら "-")。engine/book/check_book.py が使う
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/book.h"
#include "../src/board.h"

int main(void) {
    char line[4096];
    int cur_rule = -1;
    Rule rule;
    Board b;
    board_global_init();
    while (fgets(line, sizeof line, stdin)) {
        char *tok = strtok(line, " \t\r\n");
        if (!tok) continue;
        int r = atoi(tok);
        if (r != cur_rule) {
            rule = rule_from_gomocup(r);
            pattern_init(&rule);
            cur_rule = r;
        }
        board_init(&b, 15, &rule);
        while ((tok = strtok(NULL, " \t\r\n"))) {
            int x, y;
            if (sscanf(tok, "%d,%d", &x, &y) == 2) board_move(&b, to_pos(x, y));
        }
        int p;
        if (book_probe(&b, &p)) printf("%d,%d\n", pos_x(p), pos_y(p));
        else printf("-\n");
    }
    return 0;
}
