// VCF / VCT ソルバのテスト用ツール
//
//   g2-tss solve <rule> <vcf|vct> [depth] [nodes]
//       標準入力の各行 (黒から順の手 "x,y x,y ...") について手番側の詰みを調べ、1 行ずつ出力:
//       "win <初手> | <手順> | nodes=N"  または  "nowin | nodes=N [aborted]"
//   g2-tss opp <rule> <vcf|vct> [depth] [nodes]
//       同様に、手番でない側 (パスしたら) の詰みを調べる
//   g2-tss defend <rule> [nodes]
//       相手の VCF を崩す手番側の手 (vcf_defenses) を出力
//   g2-tss puzzles <file> [nodes]
//       問題集を解いて正誤を数える。書式は tests/puzzles/tactics.txt の先頭
// rule は Gomocup 番号 (0/1/4) か class<check>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../src/board.h"
#include "../src/vcf.h"

static Rule parse_rule(const char *s) {
    if (!strncmp(s, "class", 5)) return rule_from_class_check(atoi(s + 5));
    if (!strcmp(s, "renju")) return rule_from_gomocup(4);
    if (!strcmp(s, "freestyle")) return rule_from_gomocup(0);
    if (!strcmp(s, "standard")) return rule_from_gomocup(1);
    return rule_from_gomocup(atoi(s));
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// "x,y x,y ..." を盤に打つ。'#' 以降は無視。打てない手があれば -1
static int load_moves(Board *b, char *s) {
    char *hash = strchr(s, '#');
    if (hash) *hash = 0;
    for (char *tok = strtok(s, " \t\r\n"); tok; tok = strtok(NULL, " \t\r\n")) {
        int x, y;
        if (sscanf(tok, "%d,%d", &x, &y) != 2) continue;
        if (!board_on(b, x, y) || b->cell[to_pos(x, y)] != EMPTY) return -1;
        board_move(b, to_pos(x, y));
    }
    return 0;
}

static void print_line(const TssResult *r) {
    for (int i = 0; i < r->nline; i++) printf("%s%d,%d", i ? " " : "", pos_x(r->line[i]), pos_y(r->line[i]));
}

static int cmd_solve(Rule rule, int mode, int depth, long nodes, int opp) {
    static Board b;
    char line[8192];
    double total = 0;
    while (fgets(line, sizeof line, stdin)) {
        board_init(&b, 15, &rule);
        if (load_moves(&b, line) < 0) { printf("error\n"); continue; }
        int att = opp ? opp_color(board_side(&b)) : board_side(&b);
        TssResult r;
        double t0 = now_sec();
        tss_solve(&b, att, mode, depth, nodes, &r);
        total += now_sec() - t0;
        if (r.win) {
            printf("win %d,%d | ", pos_x(r.move), pos_y(r.move));
            print_line(&r);
            printf(" | nodes=%ld\n", r.nodes);
        } else {
            printf("nowin | nodes=%ld%s\n", r.nodes, r.aborted ? " aborted" : "");
        }
        fflush(stdout);
    }
    fprintf(stderr, "time %.3f s\n", total);
    return 0;
}

static int cmd_defend(Rule rule, long nodes) {
    static Board b;
    char line[8192];
    while (fgets(line, sizeof line, stdin)) {
        board_init(&b, 15, &rule);
        if (load_moves(&b, line) < 0) { printf("error\n"); continue; }
        int mv[MAX_MOVES];
        int n = vcf_defenses(&b, nodes, mv);
        if (n < 0) { printf("none\n"); continue; }
        printf("%d |", n);
        for (int i = 0; i < n; i++) printf(" %d,%d", pos_x(mv[i]), pos_y(mv[i]));
        printf("\n");
    }
    return 0;
}

// 問題集: 1 行 1 問 "<rule> <vcf|vct> <win|nowin> [first=x,y/x,y] [depth=N] : 手 ...  # コメント"
static int cmd_puzzles(const char *file, long nodes) {
    FILE *fp = fopen(file, "r");
    if (!fp) { perror(file); return 2; }
    static Board b;
    char line[8192];
    int total = 0, pass = 0, lineno = 0;
    long total_nodes = 0;
    double t_all = 0;
    int cur_rule = -100;
    while (fgets(line, sizeof line, fp)) {
        lineno++;
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == 0) continue;
        char *colon = strchr(p, ':');
        if (!colon) continue;
        *colon = 0;
        char comment[256] = "";
        char *hash = strchr(colon + 1, '#');
        if (hash) { snprintf(comment, sizeof comment, "%s", hash + 1); comment[strcspn(comment, "\r\n")] = 0; }
        char rule_s[32], mode_s[16], expect_s[16];
        if (sscanf(p, "%31s %15s %15s", rule_s, mode_s, expect_s) != 3) continue;
        int depth = 0;
        char first_s[128] = "";
        for (char *tok = strtok(p, " \t"); tok; tok = strtok(NULL, " \t")) {
            if (!strncmp(tok, "depth=", 6)) depth = atoi(tok + 6);
            if (!strncmp(tok, "first=", 6)) snprintf(first_s, sizeof first_s, "%s", tok + 6);
        }
        Rule rule = parse_rule(rule_s);
        int rule_id = rule.forbid_33 * 1 + rule.forbid_44 * 2 + rule.forbid_overline * 4 +
                      rule.black_exact_five * 8 + rule.white_exact_five * 16;
        if (rule_id != cur_rule) { pattern_init(&rule); tss_clear(); cur_rule = rule_id; }
        board_init(&b, 15, &rule);
        if (load_moves(&b, colon + 1) < 0) { printf("line %d: bad moves\n", lineno); continue; }
        int mode = !strcmp(mode_s, "vct") ? TSS_VCT : TSS_VCF;
        if (!depth) depth = mode == TSS_VCF ? 30 : 10;
        int expect = !strcmp(expect_s, "win");
        TssResult r;
        tss_clear();
        double t0 = now_sec();
        tss_solve(&b, board_side(&b), mode, depth, nodes, &r);
        double dt = now_sec() - t0;
        t_all += dt;
        total_nodes += r.nodes;
        int ok = (r.win == expect);
        if (ok && r.win && first_s[0]) {
            // first= の候補のどれかと一致
            int hit = 0;
            char buf[128];
            snprintf(buf, sizeof buf, "%s", first_s);
            for (char *t = strtok(buf, "/"); t; t = strtok(NULL, "/")) {
                int x, y;
                if (sscanf(t, "%d,%d", &x, &y) == 2 && to_pos(x, y) == r.move) hit = 1;
            }
            ok = hit;
        }
        total++;
        pass += ok;
        printf("%-4s line %3d %s %s expect=%s got=%s", ok ? "ok" : "FAIL", lineno, rule_s, mode_s, expect_s,
               r.win ? "win" : (r.aborted ? "abort" : "nowin"));
        if (r.win) printf(" first=%d,%d", pos_x(r.move), pos_y(r.move));
        printf(" nodes=%ld %.1fms  #%s\n", r.nodes, dt * 1000, comment);
    }
    fclose(fp);
    printf("passed %d / %d  (nodes %ld, %.3f s)\n", pass, total, total_nodes, t_all);
    return pass == total ? 0 : 1;
}

int main(int argc, char **argv) {
    board_global_init();
    if (argc < 3) {
        fprintf(stderr, "usage: g2-tss solve|opp <rule> <vcf|vct> [depth] [nodes] | defend <rule> [nodes] | puzzles <file> [nodes]\n");
        return 2;
    }
    if (!strcmp(argv[1], "puzzles")) return cmd_puzzles(argv[2], argc > 3 ? atol(argv[3]) : 2000000);
    Rule rule = parse_rule(argv[2]);
    pattern_init(&rule);
    if (!strcmp(argv[1], "defend")) return cmd_defend(rule, argc > 3 ? atol(argv[3]) : 200000);
    if (argc < 4) return 2;
    int mode = !strcmp(argv[3], "vct") ? TSS_VCT : TSS_VCF;
    int depth = argc > 4 ? atoi(argv[4]) : (mode == TSS_VCF ? 30 : 10);
    long nodes = argc > 5 ? atol(argv[5]) : 1000000;
    if (!strcmp(argv[1], "solve")) return cmd_solve(rule, mode, depth, nodes, 0);
    if (!strcmp(argv[1], "opp")) return cmd_solve(rule, mode, depth, nodes, 1);
    fprintf(stderr, "unknown command\n");
    return 2;
}
