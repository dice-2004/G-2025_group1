// Gomocup (piskvork) プロトコル アダプタ
//
// 授業の枠組み (calcPutPos) で書かれた AI を、Gomocup 互換エンジンとして
// 動かすためのラッパー。bench/match.py などのハーネスや、他の Gomocup
// エンジン (Rapfi 等) との対戦に使う。
//
// 使い方:  ./bench/build/pbrain-legacy [--data DIR] [--verbose]
//   --data DIR : config.txt 以外の AI データ (patterns*.csv, opening_book.csv,
//                learning_data.dat) をコピーしてくる元ディレクトリ (既定: カレント)
//   --verbose  : AI の printf 出力を stderr に流す (既定は捨てる)
//
// AI は起動ごとに一時ディレクトリへ chdir して動くので、学習による
// opening_book.csv / learning_data.dat の書き換えはリポジトリに波及しない。

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

#include "../gomoku.h"

// gomoku.c の代わりに提供するシンボル
int check = 7;
int checkOutPos(int x, int y) {
    return (x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE);
}

static FILE *proto_out;   // プロトコル出力 (元の stdout)
static int board[BOARD_SIZE][BOARD_SIZE];
static int timeout_turn_ms = 1000;
static int workdir_ready = 0;
static const char *data_dir = ".";
static char workdir[512];

static const char *data_files[] = {
    "patterns.csv", "patterns_extended.csv", "opening_book.csv",
    "learning_data.dat", NULL
};

static void copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return;
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return; }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, n, out);
    fclose(in);
    fclose(out);
}

// 初手の思考前に一度だけ作業ディレクトリと config.txt を用意する
static void prepare_workdir(void) {
    if (workdir_ready) return;
    char abs_data[4096];
    if (!realpath(data_dir, abs_data)) snprintf(abs_data, sizeof abs_data, "%s", data_dir);

    snprintf(workdir, sizeof workdir, "/tmp/pbrain-legacy-XXXXXX");
    if (!mkdtemp(workdir)) { perror("mkdtemp"); exit(1); }
    for (int i = 0; data_files[i]; i++) {
        char src[4200], dst[1024];
        snprintf(src, sizeof src, "%s/%s", abs_data, data_files[i]);
        snprintf(dst, sizeof dst, "%s/%s", workdir, data_files[i]);
        copy_file(src, dst);
    }
    char cfg[1024];
    snprintf(cfg, sizeof cfg, "%s/config.txt", workdir);
    FILE *fp = fopen(cfg, "w");
    if (fp) {
        // ID は TIMEOUT_MS の 8 割で打ち切るが、実測で超過することがあるので控えめに
        int t = timeout_turn_ms * 6 / 10;
        if (t < 50) t = 50;
        const char *depth = getenv("LEGACY_DEPTH");
        fprintf(fp, "SEARCH_DEPTH=%s\n", depth ? depth : "8");
        fprintf(fp, "TRANSPOSITION_TABLE_SIZE=262144\n");
        fprintf(fp, "DEBUG_MODE=0\nDEBUG_LEVEL=0\n");
        fprintf(fp, "TIMEOUT_MS=%d\n", t);
        fprintf(fp, "LEARNING_MODE=PRODUCTION\n");
        fprintf(fp, "ITERATIVE_DEEPENING=1\n");
        fclose(fp);
    }
    if (chdir(workdir) != 0) { perror("chdir"); exit(1); }
    workdir_ready = 1;
}

static void cleanup_workdir(void) {
    if (!workdir_ready) return;
    for (int i = 0; data_files[i]; i++) unlink(data_files[i]);
    unlink("config.txt");
    if (chdir("/") == 0) rmdir(workdir);
}

static int stone_count(void) {
    int n = 0;
    for (int y = 0; y < BOARD_SIZE; y++)
        for (int x = 0; x < BOARD_SIZE; x++)
            if (board[y][x] != STONE_SPACE) n++;
    return n;
}

static void think_and_reply(void) {
    prepare_workdir();
    int com = (stone_count() % 2 == 0) ? STONE_BLACK : STONE_WHITE;
    int x = -1, y = -1;
    int copy[BOARD_SIZE][BOARD_SIZE];
    memcpy(copy, board, sizeof copy);
    calcPutPos(copy, com, &x, &y);
    if (!checkOutPos(x, y) || board[y][x] != STONE_SPACE) {
        fprintf(proto_out, "MESSAGE illegal move from AI (%d,%d)\n", x, y);
    } else {
        board[y][x] = com;
    }
    fprintf(proto_out, "%d,%d\n", x, y);
    fflush(proto_out);
}

static void put_opponent(int x, int y) {
    if (!checkOutPos(x, y) || board[y][x] != STONE_SPACE) return;
    int n = stone_count();
    board[y][x] = (n % 2 == 0) ? STONE_BLACK : STONE_WHITE;
}

int main(int argc, char **argv) {
    int verbose = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--data") && i + 1 < argc) data_dir = argv[++i];
        else if (!strcmp(argv[i], "--verbose")) verbose = 1;
    }

    // AI 内部の printf がプロトコルを壊さないよう stdout を差し替える
    int fd = dup(1);
    proto_out = fdopen(fd, "w");
    int sink = verbose ? dup(2) : open("/dev/null", O_WRONLY);
    dup2(sink, 1);
    close(sink);

    char line[256];
    while (fgets(line, sizeof line, stdin)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!strncasecmp(line, "START", 5)) {
            int size = atoi(line + 5);
            if (size != BOARD_SIZE) {
                fprintf(proto_out, "ERROR only %d board is supported\n", BOARD_SIZE);
            } else {
                memset(board, 0, sizeof board);
                fprintf(proto_out, "OK\n");
            }
        } else if (!strncasecmp(line, "RESTART", 7)) {
            memset(board, 0, sizeof board);
            fprintf(proto_out, "OK\n");
        } else if (!strncasecmp(line, "INFO", 4)) {
            char key[64];
            long val;
            if (sscanf(line + 4, "%63s %ld", key, &val) == 2) {
                if (!strcasecmp(key, "timeout_turn") && val > 0) timeout_turn_ms = (int)val;
                else if (!strcasecmp(key, "rule")) { /* 授業ルールのみ対応 */ }
            }
            continue;
        } else if (!strncasecmp(line, "BEGIN", 5)) {
            think_and_reply();
            continue;
        } else if (!strncasecmp(line, "TURN", 4)) {
            int x, y;
            if (sscanf(line + 4, " %d,%d", &x, &y) == 2) put_opponent(x, y);
            think_and_reply();
            continue;
        } else if (!strncasecmp(line, "BOARD", 5)) {
            memset(board, 0, sizeof board);
            // BOARD の石は own=1 / opp=2。手番側 (自分) の色は石数の偶奇で決まる
            int xs[BOARD_SIZE * BOARD_SIZE], ys[BOARD_SIZE * BOARD_SIZE], cs[BOARD_SIZE * BOARD_SIZE];
            int n = 0;
            while (fgets(line, sizeof line, stdin)) {
                line[strcspn(line, "\r\n")] = 0;
                if (!strncasecmp(line, "DONE", 4)) break;
                int x, y, c;
                if (sscanf(line, "%d,%d,%d", &x, &y, &c) == 3 && n < BOARD_SIZE * BOARD_SIZE) {
                    xs[n] = x; ys[n] = y; cs[n] = c; n++;
                }
            }
            int me = (n % 2 == 0) ? STONE_BLACK : STONE_WHITE;
            int opp = (me == STONE_BLACK) ? STONE_WHITE : STONE_BLACK;
            for (int i = 0; i < n; i++)
                if (checkOutPos(xs[i], ys[i])) board[ys[i]][xs[i]] = (cs[i] == 1) ? me : opp;
            think_and_reply();
            continue;
        } else if (!strncasecmp(line, "ABOUT", 5)) {
            fprintf(proto_out, "name=\"G2025-legacy\", version=\"3.4\", author=\"G-2025 group1\"\n");
        } else if (!strncasecmp(line, "END", 3)) {
            break;
        } else if (line[0]) {
            fprintf(proto_out, "UNKNOWN %s\n", line);
        }
        fflush(proto_out);
    }
    cleanup_workdir();
    return 0;
}
