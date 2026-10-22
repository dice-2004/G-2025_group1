// G2 エンジンのテスト用ツール
//
//   g2-test selftest [rule]      ランダムに打って戻す操作を繰り返し、差分更新が作り直しと一致するか
//   g2-test forbid <rule>        標準入力の各行 (黒から順の手 "x,y x,y ...") について、
//                                黒が打つと禁じ手になる空点を "x,y:F" 形式で出力 (Python 照合用)
//   g2-test bench [rule]         パターン表生成と着手/戻しの速度
// rule は Gomocup 番号 (0/1/4) か class<check> (例 class7)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../src/board.h"

static Rule parse_rule(const char *s) {
    if (!s) return rule_from_gomocup(4);
    if (!strncmp(s, "class", 5)) return rule_from_class_check(atoi(s + 5));
    return rule_from_gomocup(atoi(s));
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static int random_empty_near(Board *b) {
    for (int tries = 0; tries < 1000; tries++) {
        int p;
        if (b->nmoves == 0) {
            p = to_pos(b->size / 2, b->size / 2);
        } else {
            int base = b->moves[rand() % b->nmoves];
            p = base + (rand() % 5 - 2) * STRIDE + (rand() % 5 - 2);
        }
        if (b->cell[p] == EMPTY) return p;
    }
    return -1;
}

static int selftest(Rule rule) {
    static Board b;
    pattern_init(&rule);
    int checks = 0;
    for (int game = 0; game < 300; game++) {
        board_init(&b, 15, &rule);
        int target = 20 + rand() % 120;
        while (b.nmoves < target) {
            int p = random_empty_near(&b);
            if (p < 0) break;
            board_move(&b, p);
            if (rand() % 4 == 0) {
                board_undo(&b);
                if (!board_verify(&b)) { printf("FAIL after undo (game %d, move %d)\n", game, b.nmoves); return 1; }
                board_move(&b, p);
            }
            if (!board_verify(&b)) { printf("FAIL after move (game %d, move %d)\n", game, b.nmoves); return 1; }
            checks++;
        }
        while (b.nmoves) board_undo(&b);
        if (!board_verify(&b)) { printf("FAIL after full undo (game %d)\n", game); return 1; }
    }
    printf("selftest OK (%d positions verified)\n", checks);
    return 0;
}

static int forbid(Rule rule) {
    static Board b;
    pattern_init(&rule);
    char line[8192];
    while (fgets(line, sizeof line, stdin)) {
        board_init(&b, 15, &rule);
        char *s = strtok(line, " \t\r\n");
        while (s) {
            int x, y;
            if (sscanf(s, "%d,%d", &x, &y) == 2) board_move(&b, to_pos(x, y));
            s = strtok(NULL, " \t\r\n");
        }
        int first = 1;
        for (int y = 0; y < 15; y++) {
            for (int x = 0; x < 15; x++) {
                int p = to_pos(x, y);
                if (b.cell[p] == EMPTY && board_is_forbidden(&b, p)) {
                    printf("%s%d,%d", first ? "" : " ", x, y);
                    first = 0;
                }
            }
        }
        printf("\n");
    }
    return 0;
}

static int bench(Rule rule) {
    static Board b;
    double t0 = now_sec();
    pattern_init(&rule);
    double t1 = now_sec();
    long ops = 0;
    board_init(&b, 15, &rule);
    srand(1);
    double t2 = now_sec();
    while (now_sec() - t2 < 1.0) {
        board_init(&b, 15, &rule);
        for (int i = 0; i < 60; i++) {
            int p = random_empty_near(&b);
            if (p < 0) break;
            board_move(&b, p);
            ops++;
        }
        while (b.nmoves) { board_undo(&b); ops++; }
    }
    double t3 = now_sec();
    printf("pattern_init: %.3f s, move/undo: %.0f ops/s\n", t1 - t0, ops / (t3 - t2));
    return 0;
}

int main(int argc, char **argv) {
    board_global_init();
    srand(12345);
    if (argc < 2) {
        fprintf(stderr, "usage: g2-test selftest|forbid|bench [rule]\n");
        return 2;
    }
    Rule rule = parse_rule(argc > 2 ? argv[2] : NULL);
    if (!strcmp(argv[1], "selftest")) return selftest(rule);
    if (!strcmp(argv[1], "forbid")) return forbid(rule);
    if (!strcmp(argv[1], "bench")) return bench(rule);
    fprintf(stderr, "unknown command\n");
    return 2;
}
