// G2 エンジンのテスト用ツール
//
//   g2-test selftest [rule]      ランダムに打って戻す操作を繰り返し、差分更新が作り直しと一致するか
//   g2-test forbid <rule>        標準入力の各行 (黒から順の手 "x,y x,y ...") について、
//                                黒が打つと禁じ手になる空点を "x,y:F" 形式で出力 (Python 照合用)
//   g2-test bench [rule]         パターン表生成と着手/戻しの速度
//   g2-test timed <rule> <ms> [threads] 標準入力の各行を時間を決めて探索し "手 点 深さ ノード数 ms 理由" を出す
//   g2-test speed <rule> <depth> 標準入力の各行 (手順 "x,y x,y ...") を固定深さで探索し、ノード数・時間・nps を出す
//   g2-test features <rule>      標準入力の各行 "結果 x,y x,y ..." (結果は黒から見て 1/0/-1) の静かな局面ごとに
//                                評価の特徴量 (空点の線パターン数・Pattern4 数を手番側/相手側) を CSV で出す (tune_eval.py 用)
// rule は Gomocup 番号 (0/1/4) か class<check> (例 class7)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../src/board.h"
#include "../src/eval.h"
#include "../src/search.h"
#include "../src/dfpn.h"
#include "../src/tz.h"
#include "../src/vcf.h"

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

// 環境変数 G2_PARAMS="tz=1,tz_depth=2" で探索の値を変える (speed / timed)
static void apply_env_params(Rule rule) {
    const char *env = getenv("G2_PARAMS");
    if (!env) return;
    char buf[512], *save = NULL;
    snprintf(buf, sizeof buf, "%s", env);
    for (char *t = strtok_r(buf, ",", &save); t; t = strtok_r(NULL, ",", &save)) {
        char *eq = strchr(t, '=');
        if (!eq) continue;
        *eq = 0;
        if (!search_set_param(t, atol(eq + 1))) fprintf(stderr, "unknown param %s\n", t);
    }
    pattern_init(&rule);
}

// 固定深さの探索速度 (詰み探索は使わずに本探索だけを測る)
static int speed(Rule rule, int depth, int threads) {
    static Board b;
    char line[8192];
    long total_nodes = 0;
    double total_ms = 0;
    int npos = 0;
    pattern_init(&rule);
    search_set_param("vct_pct", 0);
    search_set_threads(threads);
    apply_env_params(rule);
    while (fgets(line, sizeof line, stdin)) {
        board_init(&b, 15, &rule);
        char *s = line;
        int x, y, k;
        while (sscanf(s, " %d,%d%n", &x, &y, &k) == 2) { board_move(&b, to_pos(x, y)); s += k; }
        search_new_game();
        SearchLimits lim = {0};
        lim.max_depth = depth;
        SearchResult r;
        double t0 = now_sec();
        search_root(&b, &lim, &r);
        double ms = (now_sec() - t0) * 1000;
        printf("pos %2d: move %d,%d score %6d depth %d nodes %9ld time %7.1f ms (%s)\n", npos, pos_x(r.move), pos_y(r.move),
               r.score, r.depth, r.nodes, ms, r.reason);
        total_nodes += r.nodes;
        total_ms += ms;
        npos++;
    }
    printf("total: %d positions, nodes %ld, time %.1f ms, nps %.0f\n", npos, total_nodes, total_ms,
           total_ms > 0 ? total_nodes / total_ms * 1000 : 0);
    extern long nmp_stat[4];
    if (nmp_stat[0]) printf("nmp: want %ld safe %ld tried %ld cut %ld\n", nmp_stat[0], nmp_stat[1], nmp_stat[2], nmp_stat[3]);
    TzStats z = tz_stats();
    if (z.probes) printf("tz: probes %ld cache %ld replay %ld solves %ld found %ld\n", z.probes, z.cache_hits, z.replay_hits, z.solves, z.found);
    return 0;
}


// 脅威圏の健全性: 局面列 (1 行 1 局面) について tz_probe が圏を返したら、圏の外の空点 (自分の四・禁じ手を除く) に
// 打っても相手の VCF が残ることを tss_solve で確かめる
static int tzcheck(Rule rule) {
    static Board b;
    char line[8192];
    long probes = 0, zones = 0, cells = 0, outside = 0, bad = 0, unknown = 0, zsum = 0;
    pattern_init(&rule);
    while (fgets(line, sizeof line, stdin)) {
        board_init(&b, 15, &rule);
        char *s = line;
        int x, y, k;
        int mv[400], nmv = 0;
        while (sscanf(s, " %d,%d%n", &x, &y, &k) == 2) { mv[nmv++] = to_pos(x, y); s += k; }
        for (int i = 0; i < nmv; i++) {
            if (b.cell[mv[i]] != EMPTY) break;
            board_move(&b, mv[i]);
            if (board_last_move_wins(&b)) break;
            int me = board_side(&b), op = opp_color(me);
            if (b.p4cnt[op][P4_FIVE] || b.p4cnt[me][P4_FIVE]) continue;
            if (!tz_gate(&b, op)) continue;
            probes++;
            TzZone z;
            if (tz_probe(&b, 2000, &z) != 1) continue;
            zones++;
            zsum += z.n;
            static uint8_t in[NCELL];
            memset(in, 0, sizeof in);
            for (int j = 0; j < z.n; j++) in[z.cells[j]] = 1;
            for (int yy = 0; yy < 15; yy++)
                for (int xx = 0; xx < 15; xx++) {
                    int q = to_pos(xx, yy);
                    if (b.cell[q] != EMPTY) continue;
                    cells++;
                    if (in[q] || b.pat4[me][q] >= P4_BLOCK4) continue;
                    if (me == BLACK && rule_has_forbidden(&b.rule) && board_is_forbidden(&b, q)) continue;
                    if (!b.near[q] && (q % 7)) continue;   // 遠い点は一部だけ
                    outside++;
                    board_move(&b, q);
                    TssResult r;
                    tss_solve(&b, op, TSS_VCF, 40, 200000, &r);
                    board_undo(&b);
                    if (!r.win) {
                        if (r.aborted) unknown++;
                        else {
                            bad++;
                            if (bad <= 10) {
                                printf("BAD: move %d,%d outside zone breaks VCF. position:", xx, yy);
                                for (int t = 0; t < b.nmoves; t++) printf(" %d,%d", pos_x(b.moves[t]), pos_y(b.moves[t]));
                                printf("\n");
                            }
                        }
                    }
                }
        }
    }
    printf("tzcheck: probes %ld zones %ld avg_zone %.1f empty_cells %ld checked_outside %ld bad %ld unknown %ld\n", probes,
           zones, zones ? (double)zsum / zones : 0, cells, outside, bad, unknown);
    return bad > 0;
}

static void cell_features(const Board *b, int color, int *f) {
    // f[0..P_COUNT-1]: 線パターンの数、f[P_COUNT..]: Pattern4 の数 (評価と同じ補正)
    memset(f, 0, sizeof(int) * (P_COUNT + P4_COUNT));
    for (int y = 0; y < b->size; y++)
        for (int x = 0; x < b->size; x++) {
            int q = to_pos(x, y);
            if (b->cell[q] != EMPTY) continue;
            int p4 = eval_cell_class(&b->rule, color, b->pat[color][q]);
            if (p4 < 0) continue;
            f[P_COUNT + p4]++;
            for (int d = 0; d < 4; d++) f[b->pat[color][q][d]]++;
        }
}

static int features(Rule rule) {
    static Board b;
    char line[8192];
    int game = 0;
    pattern_init(&rule);
    while (fgets(line, sizeof line, stdin)) {
        int moves[MAX_MOVES], n = 0, result, off;
        if (sscanf(line, "%d%n", &result, &off) != 1) continue;
        char *s = line + off;
        int x, y, k;
        while (n < MAX_MOVES && sscanf(s, " %d,%d%n", &x, &y, &k) == 2) { moves[n++] = to_pos(x, y); s += k; }
        board_init(&b, 15, &rule);
        for (int i = 0; i < n; i++) {
            if (i >= 8 && i < n - 2) {
                int me = board_side(&b), op = opp_color(me);
                int quiet = !b.p4cnt[me][P4_FIVE] && !b.p4cnt[op][P4_FIVE] && !b.p4cnt[me][P4_FLEX4] && !b.p4cnt[op][P4_FLEX4];
                if (quiet) {
                    int f[2][P_COUNT + P4_COUNT];
                    cell_features(&b, me, f[0]);
                    cell_features(&b, op, f[1]);
                    int rs = me == BLACK ? result : -result;
                    printf("%d,%d,%d,%d", game, i, me, rs);
                    for (int c = 0; c < 2; c++)
                        for (int j = 0; j < P_COUNT + P4_COUNT; j++) printf(",%d", f[c][j]);
                    // 脅威のビット (手番側, 相手側)
                    int bits[2] = {eval_threat_bits(b.p4cnt[me]), eval_threat_bits(b.p4cnt[op])};
                    for (int c = 0; c < 2; c++)
                        for (int k = 0; k < THREAT_COUNT; k++) printf(",%d", bits[c] >> k & 1);
                    printf("\n");
                }
            }
            board_move(&b, moves[i]);
        }
        game++;
    }
    return 0;
}

// 各行の局面で、近くの空点に打った後の静的評価 (打った側から見た値) を良い順に出す (評価の確認用)
static int evalmoves(Rule rule) {
    static Board b;
    char line[8192];
    pattern_init(&rule);
    while (fgets(line, sizeof line, stdin)) {
        board_init(&b, 15, &rule);
        char *s = line;
        int x, y, k;
        while (sscanf(s, " %d,%d%n", &x, &y, &k) == 2) { board_move(&b, to_pos(x, y)); s += k; }
        int mv[MAX_MOVES], val[MAX_MOVES], n = 0;
        for (int q = 0; q < NCELL; q++) {
            if (b.cell[q] != EMPTY || !b.near[q]) continue;
            board_move(&b, q);
            mv[n] = q;
            val[n++] = -evaluate(&b);
            board_undo(&b);
        }
        for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++)
                if (val[j] > val[i]) { int t = val[i]; val[i] = val[j]; val[j] = t; t = mv[i]; mv[i] = mv[j]; mv[j] = t; }
        printf("static %d:", evaluate(&b));
        for (int i = 0; i < n && i < 15; i++) printf(" %d,%d=%d", pos_x(mv[i]), pos_y(mv[i]), val[i]);
        printf("\n");
    }
    return 0;
}

// λ3 の脅威の数: 各行の局面で、手番側の候補 w ごとに、相手の手 m のうち「m の後に手番側がパスすると相手に VCT
// (df-pn) がある」ものの数を出す。lambda3 <rule> <1 回の ms (負ならノード数)> <候補の数>
static int lambda3(Rule rule, int ms, int ncand) {
    static Board b;
    char line[8192];
    pattern_init(&rule);
    while (fgets(line, sizeof line, stdin)) {
        board_init(&b, 15, &rule);
        char *s = line;
        int x, y, k;
        while (sscanf(s, " %d,%d%n", &x, &y, &k) == 2) { board_move(&b, to_pos(x, y)); s += k; }
        int me = board_side(&b), op = opp_color(me);
        int cand[MAX_MOVES], nc = 0;
        for (int q = 0; q < NCELL; q++)
            if (b.cell[q] == EMPTY && b.near[q]) cand[nc++] = q;
        for (int i = 0; i < nc && i < ncand; i++) {
            int w = cand[i];
            board_move(&b, w);
            DfpnResult r;
            int direct = ms < 0 ? dfpn_vct(&b, op, -ms * 4L, 0, &r) : dfpn_vct(&b, op, 1L << 40, ms * 4, &r);
            int cnt = 0, tot = 0;
            char buf[2048] = "";
            if (!direct) {
                for (int q = 0; q < NCELL; q++) {
                    if (b.cell[q] != EMPTY || !b.near[q]) continue;
                    if (op == BLACK && rule_has_forbidden(&b.rule) && board_is_forbidden(&b, q)) continue;
                    board_move(&b, q);
                    tot++;
                    if (ms < 0 ? dfpn_vct(&b, op, -ms, 0, &r) : dfpn_vct(&b, op, 1L << 40, ms, &r)) {
                        cnt++;
                        snprintf(buf + strlen(buf), sizeof buf - strlen(buf), " %d,%d", pos_x(q), pos_y(q));
                    }
                    board_undo(&b);
                }
            }
            printf("%d,%d: %s threats %d / %d%s\n", pos_x(w), pos_y(w), direct ? "VCT" : "-", cnt, tot, buf);
            fflush(stdout);
            board_undo(&b);
        }
    }
    return 0;
}

// 時間を決めて探索し、各行 "move score depth nodes ms" を出す (詰みを何局面で見つけるかなどの測定用)
static int timed(Rule rule, int ms, int threads) {
    static Board b;
    char line[8192];
    pattern_init(&rule);
    search_set_threads(threads);
    apply_env_params(rule);
    while (fgets(line, sizeof line, stdin)) {
        board_init(&b, 15, &rule);
        char *s = line;
        int x, y, k;
        while (sscanf(s, " %d,%d%n", &x, &y, &k) == 2) { board_move(&b, to_pos(x, y)); s += k; }
        search_new_game();
        SearchLimits lim = {0};
        lim.hard_ms = ms;
        lim.soft_ms = ms * search_soft_pct() / 100;
        SearchResult r;
        search_root(&b, &lim, &r);
        printf("%d,%d %d %d %ld %d %s\n", pos_x(r.move), pos_y(r.move), r.score, r.depth, r.nodes, r.ms, r.reason);
        fflush(stdout);
    }
    return 0;
}

// 4 方向のパターンを小さい順に並べた組の添字 (方向によらない「組み合わせ」の番号)
static int sorted_pidx(const uint8_t pats[4]) {
    uint8_t s[4] = {pats[0], pats[1], pats[2], pats[3]};
    for (int i = 1; i < 4; i++)
        for (int j = i; j > 0 && s[j - 1] > s[j]; j--) { uint8_t t = s[j]; s[j] = s[j - 1]; s[j - 1] = t; }
    return pattern_index(s);
}

// 評価の表の調整用: 標準入力の各行 "手順<TAB>ラベル" の局面ごとに
// "ラベル 手番 静か 評価 | 手番側の組:数 ... | 相手側の組:数 ..." を出す (組 = sorted_pidx、禁じ手の点は除く)
static int msdump(Rule rule) {
    static Board b;
    static char line[16384];
    g_eval_params.use_table = 0;   // 補正なしの評価に対して補正を合わせる
    pattern_init(&rule);
    while (fgets(line, sizeof line, stdin)) {
        char *tab = strchr(line, '\t');
        if (!tab) continue;
        *tab = 0;
        char label[64];
        if (sscanf(tab + 1, "%63s", label) != 1) continue;
        board_init(&b, 15, &rule);
        char *s = line;
        int x, y, k;
        while (sscanf(s, " %d,%d%n", &x, &y, &k) == 2) { board_move(&b, to_pos(x, y)); s += k; }
        int me = board_side(&b), op = opp_color(me);
        int quiet = !b.p4cnt[me][P4_FIVE] && !b.p4cnt[op][P4_FIVE] && !b.p4cnt[me][P4_FLEX4] && !b.p4cnt[op][P4_FLEX4];
        printf("%s %d %d %d", label, me, quiet, evaluate(&b));
        for (int side = 0; side < 2; side++) {
            int color = side == 0 ? me : op;
            static int cnt[PIDX_COUNT];
            memset(cnt, 0, sizeof cnt);
            printf(" |");
            for (int q = 0; q < NCELL; q++) {
                if (b.cell[q] != EMPTY) continue;
                if (eval_cell_class(&b.rule, color, b.pat[color][q]) < 0) continue;
                int id = sorted_pidx(b.pat[color][q]);
                if (id) cnt[id]++;
            }
            for (int i = 1; i < PIDX_COUNT; i++)
                if (cnt[i]) printf(" %d:%d", i, cnt[i]);
        }
        printf("\n");
    }
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
    if (!strcmp(argv[1], "features")) return features(rule);
    if (!strcmp(argv[1], "lambda3")) return lambda3(rule, argc > 3 ? atoi(argv[3]) : 20, argc > 4 ? atoi(argv[4]) : 400);
    if (!strcmp(argv[1], "evalmoves")) return evalmoves(rule);
    if (!strcmp(argv[1], "timed")) return timed(rule, argc > 3 ? atoi(argv[3]) : 1000, argc > 4 ? atoi(argv[4]) : 1);
    if (!strcmp(argv[1], "msdump")) return msdump(rule);
    if (!strcmp(argv[1], "tzcheck")) return tzcheck(rule);
    if (!strcmp(argv[1], "speed")) return speed(rule, argc > 3 ? atoi(argv[3]) : 8, argc > 4 ? atoi(argv[4]) : 1);
    fprintf(stderr, "unknown command\n");
    return 2;
}
