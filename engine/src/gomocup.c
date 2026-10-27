// Gomocup (piskvork) プロトコルの入口: pbrain-g2
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "board.h"
#include "eval.h"
#include "search.h"
#include "vcf.h"
#include "think.h"

static Board board;
static Rule rule;
static int rule_code = 0;       // 現在の Gomocup ルール
static int tables_rule = -1;    // パターン表を作ったときのルール
static int eval_dirty = 0;      // 評価の重みが INFO で変わった
static int size = 15;
static ThinkLimits limits = {5000, 0, 0, 0, 0};

// 表を (必要なら) 作り直す。作り直したら 1
static int ensure_tables(void) {
    if (tables_rule == rule_code) return 0;
    rule = rule_from_gomocup(rule_code);
    pattern_init(&rule);
    tss_clear();
    search_clear_tt();
    tables_rule = rule_code;
    return 1;
}

static void new_game(void) {
    ensure_tables();
    board_init(&board, size, &rule);
    search_new_game();
}

// ルールや評価の表が変わっていたら表と盤を作り直す。
// INFO を受けた時点で呼び、持ち時間の中 (TURN の後) で表を作らないようにする
static void sync_tables(void) {
    if (ensure_tables() || board.rule.gomocup_rule != rule_code) {
        int moves[MAX_MOVES], n = board.nmoves;
        memcpy(moves, board.moves, sizeof(int) * n);
        board_init(&board, size, &rule);
        for (int i = 0; i < n; i++) board_move(&board, moves[i]);
    }
}

// INFO や START の処理 (表の作成・置換表の確保) にかかった時間。審判は INFO の直後に TURN/BOARD を
// 送ってくるので、この時間は次の手の持ち時間から引く
static double pending_ms = 0;

static double wall_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec * 1e-6;
}

static void reply_move(void) {
    double t0 = wall_ms();
    sync_tables();
    ThinkLimits lim = limits;
    double used = pending_ms + (wall_ms() - t0);
    if (lim.time_ms > 0) lim.time_ms -= (int)used;
    if (lim.time_ms < 0) lim.time_ms = 1;   // 0 は「指定なし」なので最小の 1ms にする
    pending_ms = 0;
    int p = think_move(&board, &lim);
    if (p < 0) {
        printf("ERROR no move\n");
    } else {
        board_move(&board, p);
        printf("%d,%d\n", pos_x(p), pos_y(p));
    }
    fflush(stdout);
}

static int parse_xy(const char *s, int *x, int *y) {
    return sscanf(s, " %d , %d", x, y) == 2 && board_on(&board, *x, *y);
}

int main(void) {
    char line[256];
    srand((unsigned)time(NULL));
    board_global_init();
    setvbuf(stdout, NULL, _IOLBF, 0);

    while (fgets(line, sizeof line, stdin)) {
        line[strcspn(line, "\r\n")] = 0;
        double cmd_t0 = wall_ms();
        char *arg = line;
        while (*arg && *arg != ' ') arg++;
        if (*arg) *arg++ = 0;

        if (!strcasecmp(line, "START")) {
            int s = atoi(arg);
            if (s < 5 || s > MAX_SIZE) {
                printf("ERROR unsupported size %d\n", s);
            } else {
                size = s;
                new_game();
                printf("OK\n");
            }
        } else if (!strcasecmp(line, "RESTART")) {
            new_game();
            printf("OK\n");
        } else if (!strcasecmp(line, "INFO")) {
            char key[64];
            long val;
            if (sscanf(arg, "%63s %ld", key, &val) == 2) {
                if (!strcasecmp(key, "timeout_turn")) limits.time_ms = (int)val;
                else if (!strcasecmp(key, "time_left")) limits.time_left_ms = val > 1000000000 ? 1000000000 : (int)val;
                else if (!strcasecmp(key, "max_memory") && val > 0) {
                    // 置換表は max_memory の 1/4 まで (上限 64MB。大きいとページの割り当てに時間がかかり、
                    // 1 手 1 秒程度の探索では 32〜64MB で足りる)
                    long mb = val / 4 / (1 << 20);
                    search_set_tt_size((int)(mb < 4 ? 4 : mb > 64 ? 64 : mb));
                    search_new_game();
                }
                else if (!strcasecmp(key, "max_depth")) limits.max_depth = (int)val;
                else if (!strcasecmp(key, "max_node")) limits.max_nodes = val;
                else if (!strcasecmp(key, "verbose")) limits.verbose = (int)val;
                else if (!strncasecmp(key, "p_", 2) && search_set_param(key + 2, val)) eval_dirty = 1;
                else if (!strcasecmp(key, "rule")) rule_code = (int)val & 5;  // 1=ちょうど五, 4=連珠
                if (eval_dirty && tables_rule >= 0) {
                    // 評価の重みが変わった: 評価の表だけ作り直し (パターン表はそのまま)、盤の集計を作り直す
                    eval_init(&rule);
                    search_clear_tt();
                    board.rule.gomocup_rule = -1;
                }
                eval_dirty = 0;
                if (board.size) sync_tables();
            }
        } else if (!strcasecmp(line, "BEGIN")) {
            reply_move();
        } else if (!strcasecmp(line, "TURN")) {
            int x, y;
            if (parse_xy(arg, &x, &y) && board.cell[to_pos(x, y)] == EMPTY) {
                board_move(&board, to_pos(x, y));
                reply_move();
            } else {
                printf("ERROR bad TURN %s\n", arg);
            }
        } else if (!strcasecmp(line, "BOARD")) {
            // own=1 / opp=2。石の順序は手順と限らないので、黒白を交互に並べ直して置く
            int bl[MAX_MOVES], wh[MAX_MOVES], nb = 0, nw = 0, raw[MAX_MOVES][3], n = 0;
            while (fgets(line, sizeof line, stdin)) {
                line[strcspn(line, "\r\n")] = 0;
                if (!strncasecmp(line, "DONE", 4)) break;
                int x, y, c;
                if (sscanf(line, "%d,%d,%d", &x, &y, &c) == 3 && n < MAX_MOVES) {
                    raw[n][0] = x; raw[n][1] = y; raw[n][2] = c; n++;
                }
            }
            int me = (n % 2 == 0) ? BLACK : WHITE;
            for (int i = 0; i < n; i++) {
                int color = (raw[i][2] == 1) ? me : opp_color(me);
                int p = to_pos(raw[i][0], raw[i][1]);
                if (color == BLACK) bl[nb++] = p; else wh[nw++] = p;
            }
            ensure_tables();
            board_init(&board, size, &rule);
            for (int i = 0; i < nb || i < nw; i++) {
                if (i < nb) board_move(&board, bl[i]);
                if (i < nw) board_move(&board, wh[i]);
            }
            reply_move();
        } else if (!strcasecmp(line, "TAKEBACK")) {
            int x, y;
            if (parse_xy(arg, &x, &y) && board.nmoves > 0 && board.moves[board.nmoves - 1] == to_pos(x, y)) {
                board_undo(&board);
                printf("OK\n");
            } else {
                printf("ERROR bad TAKEBACK\n");
            }
        } else if (!strcasecmp(line, "ABOUT")) {
            printf("name=\"G2\", version=\"0.2\", author=\"G-2025 group1\", country=\"Japan\"\n");
        } else if (!strcasecmp(line, "END")) {
            break;
        } else if (line[0]) {
            printf("UNKNOWN %s\n", line);
        }
        if (!strcasecmp(line, "INFO") || !strcasecmp(line, "START") || !strcasecmp(line, "RESTART"))
            pending_ms += wall_ms() - cmd_t0;
        fflush(stdout);
    }
    return 0;
}
