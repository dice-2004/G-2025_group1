// VCF / VCT ソルバのテスト用ツール
//
//   g2-tss solve <rule> <vcf|vct> [depth] [nodes]
//       標準入力の各行 (黒から順の手 "x,y x,y ...") について手番側の詰みを調べ、1 行ずつ出力:
//       "win <初手> | <手順> | nodes=N"  または  "nowin | nodes=N [aborted]"
//   g2-tss opp <rule> <vcf|vct> [depth] [nodes]
//       同様に、手番でない側 (パスしたら) の詰みを調べる
//   g2-tss defend <rule> [nodes]
//       相手の VCF を崩す手番側の手 (vcf_defenses) を出力
//   g2-tss vctdefend <rule> [nodes]
//       相手の VCF/VCT を崩す手番側の手 (vct_defenses) を出力
//   g2-tss puzzles <file> [nodes]
//       問題集を解いて正誤を数える。書式は tests/puzzles/tactics.txt の先頭
// rule は Gomocup 番号 (0/1/4) か class<check>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../src/board.h"
#include "../src/dfpn.h"
#include "../src/fvcf.h"
#include "../src/search.h"
#include "../src/l3.h"
#include "../src/ts.h"
#include "../src/tz.h"
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

// df-pn の VCT (dfpn.c): g2-tss dfpn <rule> [nodes]
static int cmd_l3(Rule rule, long nodes, int opp, int ms) {
    static Board b;
    char line[8192];
    double total = 0;
    while (fgets(line, sizeof line, stdin)) {
        board_init(&b, 15, &rule);
        if (load_moves(&b, line) < 0) { printf("error\n"); continue; }
        int att = opp ? opp_color(board_side(&b)) : board_side(&b);
        L3Result r;
        double t0 = now_sec();
        l3_win(&b, att, nodes, ms, &r);
        double dt = now_sec() - t0;
        total += dt;
        if (r.win) printf("win %d,%d | nodes=%ld vct=%ld %.1fms\n", pos_x(r.move), pos_y(r.move), r.nodes, r.vct_calls, dt * 1000);
        else printf("nowin | nodes=%ld vct=%ld%s %.1fms\n", r.nodes, r.vct_calls, r.aborted ? " aborted" : "", dt * 1000);
        extern long g_l3_stat[6];
        fprintf(stderr, "  or %ld (avg ch %.1f) and %ld (avg ch %.1f) zonefail %ld dfpnfail %ld\n", g_l3_stat[0], g_l3_stat[1] / (g_l3_stat[0] + 1e-9), g_l3_stat[2], g_l3_stat[3] / (g_l3_stat[2] + 1e-9), g_l3_stat[4], g_l3_stat[5]);
        extern double g_l3_ms[4];
        fprintf(stderr, "  ms: or-vct %.0f quiet %.0f and-zone %.0f and-fail %.0f\n", g_l3_ms[0], g_l3_ms[1], g_l3_ms[2], g_l3_ms[3]);
        memset(g_l3_ms, 0, sizeof g_l3_ms);
        memset(g_l3_stat, 0, sizeof g_l3_stat);
        fflush(stdout);
    }
    fprintf(stderr, "time %.3f s\n", total);
    return 0;
}

// 調査用: 手番側の「打って相手がパスしたら VCT がある」手を数える (λ3 の脅威の数)
static int cmd_l3count(Rule rule, long nodes, int minp) {
    static Board b;
    char line[8192];
    while (fgets(line, sizeof line, stdin)) {
        board_init(&b, 15, &rule);
        if (load_moves(&b, line) < 0) { printf("error\n"); continue; }
        int att = board_side(&b), n = 0, nq = 0, now = 0;
        double t0 = now_sec();
        if (tss_solve(&b, att, TSS_VCT, 8, nodes * 4, NULL)) now = 1;
        char buf[2048]; int bl = 0; buf[0] = 0;
        for (int q = 0; q < NCELL; q++) {
            if (b.cell[q] != EMPTY || !b.near[q] || b.pat4[att][q] < minp) continue;
            if (rule_has_forbidden(&b.rule) && att == BLACK && board_is_forbidden(&b, q)) continue;
            nq++;
            board_move_color(&b, q, att);
            int ok = tss_solve(&b, att, TSS_VCT, 6, nodes, NULL);
            board_undo(&b);
            if (ok) { n++; if (bl < 2000) bl += snprintf(buf + bl, sizeof buf - bl, " %d,%d", pos_x(q), pos_y(q)); }
        }
        printf("vct_now=%d threats=%d/%d %.1fms |%s\n", now, n, nq, (now_sec() - t0) * 1000, buf);
        fflush(stdout);
    }
    return 0;
}

// 調査用: dfpn_vct の証明の圏の外に受け側の石を置いても VCT が残るかを数える
static int cmd_zonecheck(Rule rule, long nodes) {
    static Board b;
    char line[8192];
    long tested = 0, broken = 0, proofs = 0, zsum = 0, zfail = 0;
    int zc[400];
    while (fgets(line, sizeof line, stdin)) {
        board_init(&b, 15, &rule);
        if (load_moves(&b, line) < 0) continue;
        int att = board_side(&b), def = opp_color(att);
        DfpnResult r;
        if (!dfpn_vct(&b, att, nodes, 0, &r)) continue;
        int nz = dfpn_vct_zone(&b, att, zc, 400);
        if (nz < 0) { zfail++; continue; }
        proofs++; zsum += nz;
        static uint8_t in[NCELL]; memset(in, 0, sizeof in);
        for (int i = 0; i < nz; i++) in[zc[i]] = 1;
        for (int q = 0; q < NCELL; q++) {
            if (b.cell[q] != EMPTY || !b.near[q] || in[q]) continue;
            if (rule_has_forbidden(&b.rule) && def == BLACK && board_is_forbidden(&b, q)) continue;
            int pd = b.pat4[def][q];
            board_move_color(&b, q, def);
            DfpnResult r2;
            int ok = dfpn_vct(&b, att, nodes * 10, 0, &r2);
            board_undo(&b);
            tested++;
            { static long hist[2][P4_COUNT]; hist[!ok][pd]++; if (getenv("ZHIST") && tested % 50 == 0) { for (int k = 0; k < P4_COUNT; k++) fprintf(stderr, "%d:%ld/%ld ", k, hist[1][k], hist[0][k] + hist[1][k]); fprintf(stderr, "\n"); } }
            if (!ok) { broken++; if (broken <= 10) printf("broken: %s  def at %d,%d (aborted %d)\n", "pos", pos_x(q), pos_y(q), r2.aborted); }
        }
    }
    printf("proofs %ld (zone fail %ld) avg zone %.1f, outside cells tested %ld, broken %ld\n", proofs, zfail, zsum / (proofs + 1e-9), tested, broken);
    return 0;
}

static int cmd_ts(Rule rule, int depth, int ms, int opp) {
    static Board b;
    char line[8192];
    double total = 0;
    while (fgets(line, sizeof line, stdin)) {
        board_init(&b, 15, &rule);
        if (load_moves(&b, line) < 0) { printf("error\n"); continue; }
        int att = opp ? opp_color(board_side(&b)) : board_side(&b);
        TsResult r;
        double t0 = now_sec();
        ts_win(&b, att, depth, getenv("TS_NODES") ? atol(getenv("TS_NODES")) : 1L << 40, ms, &r);
        double dt = now_sec() - t0;
        total += dt;
        if (r.win && getenv("TS_ZONE")) {
            int zc[400], nz = ts_zone(&b, att, zc, 400);
            printf("zone %d:", nz);
            for (int i = 0; i < nz; i++) if (b.cell[zc[i]] == EMPTY) printf(" %d,%d", pos_x(zc[i]), pos_y(zc[i]));
            printf("\n");
        }
        if (r.win) printf("win %d,%d | depth=%d nodes=%ld %.1fms\n", pos_x(r.move), pos_y(r.move), r.depth, r.nodes, dt * 1000);
        else printf("nowin | depth=%d nodes=%ld%s %.1fms\n", r.depth, r.nodes, r.aborted ? " aborted" : "", dt * 1000);
        fflush(stdout);
    }
    fprintf(stderr, "time %.3f s\n", total);
    return 0;
}

// 調査用: 1 行目の局面で相手 (手番側) の勝ちを ts で証明して証明の手を覚え、2 行目以降を覚えた手ありで調べる
static int cmd_tssib(Rule rule, int ms) {
    static Board b;
    char line[8192];
    int first = 1, zc[400];
    ts_hint_clear();
    while (fgets(line, sizeof line, stdin)) {
        board_init(&b, 15, &rule);
        if (load_moves(&b, line) < 0) continue;
        int att = board_side(&b);
        TsResult r;
        double t0 = now_sec();
        int w = ts_win(&b, att, 40, 1L << 40, ms, &r);
        int nz = (w && first && !getenv("TS_NOHINT")) ? ts_zone(&b, att, zc, 400) : 0;
        printf("%s nodes=%ld %.1fms%s\n", w ? "win  " : "nowin", r.nodes, (now_sec() - t0) * 1000, first ? (nz >= 0 ? " (hints)" : " (no zone)") : "");
        first = 0;
        fflush(stdout);
    }
    return 0;
}

static int cmd_dfpn(Rule rule, long nodes, int opp, int ms) {
    static Board b;
    char line[8192];
    double total = 0;
    while (fgets(line, sizeof line, stdin)) {
        board_init(&b, 15, &rule);
        if (load_moves(&b, line) < 0) { printf("error\n"); continue; }
        int att = opp ? opp_color(board_side(&b)) : board_side(&b);
        DfpnResult r;
        double t0 = now_sec();
        dfpn_vct(&b, att, nodes, ms, &r);
        double dt = now_sec() - t0;
        total += dt;
        if (r.win) printf("win %d,%d | nodes=%ld %.1fms\n", pos_x(r.move), pos_y(r.move), r.nodes, dt * 1000);
        else printf("nowin | nodes=%ld%s %.1fms\n", r.nodes, r.aborted ? " aborted" : "", dt * 1000);
        fflush(stdout);
    }
    fprintf(stderr, "time %.3f s\n", total);
    TzStats z = tz_stats();
    fprintf(stderr, "tz: probes %ld cache %ld replay %ld solves %ld found %ld\n", z.probes, z.cache_hits, z.replay_hits, z.solves, z.found);
    return 0;
}

// 相手の VCT の圏の健全性: パスしたら相手に VCT があり圏が取れた局面で、圏の外の空点 (石の近く、自分の止め三以上を作る点・
// 禁じ手を除く) に打った後も相手に VCT があるかを大きいノード数で確かめる。nodes = 圏を取る探索、vnodes = 確かめ
static int cmd_vzcheck(Rule rule, long nodes, long vnodes, int pad) {
    static Board b;
    char line[8192];
    static int cells[NCELL];
    static uint8_t inz[NCELL];
    long npos = 0, nzone = 0, tested = 0, lost = 0, unknown = 0, zsize = 0, nnear = 0;
    while (fgets(line, sizeof line, stdin)) {
        board_init(&b, 15, &rule);
        char orig[8192];
        snprintf(orig, sizeof orig, "%s", line);
        if (load_moves(&b, line) < 0) continue;
        npos++;
        int me = board_side(&b), op = opp_color(me);
        if (b.p4cnt[op][P4_FLEX4]) continue;
        DfpnResult r;
        if (!dfpn_vct(&b, op, nodes, 0, &r)) continue;
        int nz = dfpn_vct_zone(&b, op, cells, NCELL);
        if (nz < 0) continue;
        nzone++;
        memset(inz, 0, sizeof inz);
        for (int i = 0; i < nz; i++) inz[cells[i]] = 1;
        if (pad)
            for (int q = 0; q < NCELL; q++)
                if (b.cell[q] == EMPTY && b.pat4[op][q] >= pad) inz[q] = 1;
        int bad = 0;
        if (getenv("VZSHOW")) {
            printf("pos %ld first %d,%d zone %d\n", npos, pos_x(r.move), pos_y(r.move), nz);
            for (int y = 0; y < 15; y++) {
                for (int x = 0; x < 15; x++) {
                    int q = to_pos(x, y);
                    putchar(b.cell[q] == BLACK ? (inz[q] ? 'X' : 'x') : b.cell[q] == WHITE ? (inz[q] ? 'O' : 'o') : inz[q] ? '+' : '.');
                }
                putchar('\n');
            }
        }
        for (int q = 0; q < NCELL; q++) {
            if (b.cell[q] != EMPTY || !b.near[q] || (me == BLACK && rule_has_forbidden(&b.rule) && board_is_forbidden(&b, q))) continue;
            nnear++;
            if (inz[q]) { zsize++; continue; }
            int a = b.pat4[me][q];
            if (a >= P4_BLOCK3 && a != P4_FLEX2_2X) continue;
            board_move(&b, q);
            int win = dfpn_vct(&b, op, vnodes, 0, &r);
            board_undo(&b);
            tested++;
            if (win) lost++;
            else {
                unknown++;
                if (bad++ < 3) printf("pos %ld: %d,%d outside zone but no VCT found%s | %s", npos, pos_x(q), pos_y(q), r.aborted ? " (aborted)" : "", orig);
            }
        }
    }
    printf("positions %ld zone %ld near-cells %ld in-zone %ld tested %ld still-lost %ld not-proven %ld\n", npos, nzone, nnear, zsize, tested, lost, unknown);
    return 0;
}

static int cmd_defend(Rule rule, long nodes, int vct) {
    static Board b;
    char line[8192];
    double total = 0;
    while (fgets(line, sizeof line, stdin)) {
        board_init(&b, 15, &rule);
        if (load_moves(&b, line) < 0) { printf("error\n"); continue; }
        int mv[MAX_MOVES];
        double t0 = now_sec();
        int n = vct ? vct_defenses(&b, 10, nodes, mv) : vcf_defenses(&b, nodes, mv);
        total += now_sec() - t0;
        if (n < 0) { printf("none\n"); continue; }
        printf("%d |", n);
        for (int i = 0; i < n; i++) printf(" %d,%d", pos_x(mv[i]), pos_y(mv[i]));
        printf("\n");
    }
    fprintf(stderr, "time %.3f s\n", total);
    return 0;
}

// 問題集: 1 行 1 問 "<rule> <vcf|vct> <win|nowin> [first=x,y/x,y] [depth=N] : 手 ...  # コメント"
// fvcf_solve と tss_solve(TSS_VCF) の結果 (勝ち・初手・手順・ノード数・打ち切り) を比べる。食い違いを数えて出力
static long fc_pos, fc_bad, fc_wins;
static double fc_t_old, fc_t_new;
static long fc_n_old, fc_n_new, fc_replays, fc_replay_ok;
static void fc_compare(Board *b, int att, int depth, long nodes, const char *tag) {
    TssResult r1, r2;
    uint64_t h = b->hash;
    double t0 = now_sec();
    tss_solve(b, att, TSS_VCF, depth, nodes, &r1);
    double t1 = now_sec();
    fvcf_solve(b, att, depth, nodes, &r2);
    double t2 = now_sec();
    fc_t_old += t1 - t0;
    fc_t_new += t2 - t1;
    fc_n_old += r1.nodes;
    fc_n_new += r2.nodes;
    fc_pos++;
    fc_wins += r1.win;
    int bad = r1.win != r2.win || r1.move != r2.move || r1.nodes != r2.nodes || r1.aborted != r2.aborted ||
              r1.nline != r2.nline || b->hash != h;
    for (int i = 0; !bad && i < r1.nline; i++) bad = r1.line[i] != r2.line[i];
    // 手順の再生 (tz_line_zone): この局面と、石を 1 つ足した局面で、tz.c と fvcf の結果を比べる
    if (!bad && r1.win && r1.nline > 0 && !getenv("FC_NOREPLAY")) {
        for (int t = 0; t < 4 && !bad; t++) {
            int extra = -1;
            if (t > 0) {
                int tries = 0;
                do extra = to_pos(rand() % b->size, rand() % b->size);
                while ((b->cell[extra] != EMPTY || !b->near[extra]) && ++tries < 50);
                if (b->cell[extra] != EMPTY) continue;
                board_move_color(b, extra, t == 1 ? att : opp_color(att));
            }
            TzZone z1, z2;
            int sv = g_fvcf;
            g_fvcf = 0;
            int a1 = tz_line_zone(b, att, r1.line, r1.nline, &z1);
            g_fvcf = 1;
            int a2 = tz_line_zone(b, att, r1.line, r1.nline, &z2);
            g_fvcf = sv;
            fc_replays++;
            fc_replay_ok += a1 != 0;
            bad = a1 != a2 || z1.n != z2.n || z1.plies != z2.plies || z1.first != z2.first;
            for (int i = 0; !bad && i < z1.n; i++) bad = z1.cells[i] != z2.cells[i];
            if (bad) printf("REPLAY MISMATCH t=%d ret %d/%d n %d/%d plies %d/%d\n", t, a1, a2, z1.n, z2.n, z1.plies, z2.plies);
            if (extra >= 0) board_undo(b);
        }
    }
    if (!bad) return;
    fc_bad++;
    if (fc_bad > 20) return;
    printf("MISMATCH %s att=%d nodes=%ld:", tag, att, nodes);
    for (int i = 0; i < b->nmoves; i++) printf(" %d,%d", pos_x(b->moves[i]), pos_y(b->moves[i]));
    printf("\n  old win=%d move=%d nodes=%ld ab=%d line=", r1.win, r1.move, r1.nodes, r1.aborted);
    print_line(&r1);
    printf("\n  new win=%d move=%d nodes=%ld ab=%d line=", r2.win, r2.move, r2.nodes, r2.aborted);
    print_line(&r2);
    printf("\n");
}

static void fc_report(void) {
    printf("positions %ld (vcf wins %ld)  mismatches %ld\n", fc_pos, fc_wins, fc_bad);
    printf("old: %ld nodes %.3f s (%.0f knps)   new: %ld nodes %.3f s (%.0f knps)   speedup %.2fx\n", fc_n_old, fc_t_old,
           fc_n_old / (fc_t_old + 1e-9) / 1000, fc_n_new, fc_t_new, fc_n_new / (fc_t_new + 1e-9) / 1000,
           fc_t_old / (fc_t_new + 1e-9));
    printf("replays %ld (valid %ld)\n", fc_replays, fc_replay_ok);
    printf("forbidden seen by fvcf: overline %ld  44 %ld  33 %ld\n", g_fvcf_forbid[0], g_fvcf_forbid[1], g_fvcf_forbid[2]);
}

// 標準入力の各行 (手順) のすべての途中局面で、両方の色を攻め側にして比べる。clear > 0 なら clear 局面ごとに置換表を消す
static int cmd_fvcfcheck(Rule rule, long nodes, int depth, int clear) {
    static Board b;
    char line[8192];
    pattern_init(&rule);
    tss_clear();
    fvcf_clear();
    long k = 0;
    while (fgets(line, sizeof line, stdin)) {
        char *tab = strchr(line, '\t');
        if (tab) *tab = 0;
        if (line[0] == '#') continue;
        int mv[MAX_MOVES], n = 0;
        board_init(&b, 15, &rule);
        if (load_moves(&b, line) < 0) continue;
        n = b.nmoves;
        for (int i = 0; i < n; i++) mv[i] = b.moves[i];
        board_init(&b, 15, &rule);
        for (int i = 0; i <= n; i++) {
            if (i) board_move(&b, mv[i - 1]);
            if (board_last_move_wins(&b)) break;
            for (int att = BLACK; att <= WHITE; att++) {
                if (clear > 0 && k++ % clear == 0) { tss_clear(); fvcf_clear(); fvcf_solve(&b, att, 1, 1, NULL); }
                fc_compare(&b, att, depth, nodes, "line");
            }
        }
    }
    fc_report();
    return fc_bad != 0;
}

// 乱数の対局 (石の近くにでたらめに打つ。黒は禁じ手を避ける) のすべての局面で比べる
static int cmd_fvcfrand(Rule rule, int games, unsigned seed, long nodes, int size) {
    static Board b;
    pattern_init(&rule);
    tss_clear();
    fvcf_clear();
    srand(seed);
    const long budgets[3] = {30, 300, nodes};
    long k = 0;
    for (int g = 0; g < games; g++) {
        board_init(&b, size, &rule);
        int plies = 20 + rand() % (size * size / 2);
        for (int ply = 0; ply < plies; ply++) {
            int cand[MAX_MOVES], nc = 0;
            for (int y = 0; y < size; y++)
                for (int x = 0; x < size; x++) {
                    int p = to_pos(x, y);
                    if (b.cell[p] != EMPTY) continue;
                    if (b.nmoves ? b.near[p] == 0 : (x != size / 2 || y != size / 2)) continue;
                    cand[nc++] = p;
                }
            int p = -1;
            for (int t = 0; t < 20 && nc > 0; t++) {
                int q = cand[rand() % nc];
                if (board_side(&b) == BLACK && board_is_forbidden(&b, q)) continue;
                p = q;
                break;
            }
            if (p < 0) break;
            board_move(&b, p);
            if (board_last_move_wins(&b)) break;
            for (int att = BLACK; att <= WHITE; att++) {
                if (k++ % 64 == 0) { tss_clear(); fvcf_clear(); fvcf_solve(&b, att, 1, 1, NULL); }
                fc_compare(&b, att, 30, budgets[rand() % 3], "rand");
            }
        }
    }
    fc_report();
    return fc_bad != 0;
}

// 調査用: 各局面で、両方の色について「打ってパスしたら VCF」の点 (λ2 の脅威の点) を数える。
// 出力: 手順の後ろの欄 (勝者など) をそのまま付けて「黒の点数 白の点数 黒の四の点 白の四の点 評価(黒から) 時間ms」
// 調査用: 局面ごとに 手番側から見た評価、黒の p4cnt[0..P4_COUNT-1]、白の p4cnt (評価の脅威の組み合わせの検討用)
static int cmd_p4dump(Rule rule) {
    static Board b;
    char line[8192];
    while (fgets(line, sizeof line, stdin)) {
        board_init(&b, 15, &rule);
        if (load_moves(&b, line) < 0) { printf("error\n"); continue; }
        printf("%d %d", board_side(&b), evaluate(&b));
        for (int c = BLACK; c <= WHITE; c++)
            for (int k = 0; k < P4_COUNT; k++) printf(" %d", b.p4cnt[c][k]);
        printf("\n");
    }
    return 0;
}

static int cmd_potential(Rule rule, long nodes, int lo) {
    static Board b;
    char line[8192];
    double total = 0;
    while (fgets(line, sizeof line, stdin)) {
        char tail[256] = "";
        char *tab = strchr(line, '\t');
        if (tab) { snprintf(tail, sizeof tail, "%s", tab + 1); tail[strcspn(tail, "\r\n")] = 0; *tab = 0; }
        board_init(&b, 15, &rule);
        if (load_moves(&b, line) < 0) continue;
        int cnt[3] = {0}, f4[3] = {0};
        double t0 = now_sec();
        for (int c = BLACK; c <= WHITE; c++) {
            for (int q = 0; q < NCELL; q++) {
                if (b.cell[q] != EMPTY || !b.near[q]) continue;
                int p4 = b.pat4[c][q];
                if (p4 >= P4_BLOCK4 && p4 != P4_FIVE) f4[c]++;
                if (p4 < lo || p4 >= P4_FLEX3) continue;
                if (c == BLACK && board_is_forbidden(&b, q)) continue;
                if (nodes <= -10) {   // 調査用: 足した後に四の手数 -nodes-10 以下の VCF があるか (ノードの上限なし、手の順に依らない)
                    uint64_t h;
                    TssResult tr;
                    if (fvcf_extra_gate(&b, c, q, &h) > 0 && fvcf_extra_solve(&b, c, q, (int)(-nodes - 10), 1L << 30, &tr)) cnt[c]++;
                } else if (nodes < 0) {   // 調査用: 足した石を通る線の空点に、四四・四三・達四の点ができるか (パターンだけ)
                    int hit = 0;
                    for (int d = 0; d < 4 && !hit; d++)
                        for (int k = -5; k <= 5 && !hit; k++) {
                            int r = q + k * DIR_OFFSET[d];
                            if (k == 0 || b.cell[r] != EMPTY) continue;
                            uint8_t pt[4];
                            memcpy(pt, b.pat[c][r], 4);
                            pt[d] = g_pattern_table[c][b.key[r][d] | (uint32_t)c << (2 * key_slot(-k))];
                            int nw = g_p4_table[pattern_index(pt)];
                            if ((nw == P4_FLEX4 || nw == P4_BLOCK4_FLEX3) && nw != b.pat4[c][r]) hit = 1;
                        }
                    cnt[c] += hit;
                } else if (nodes == 0) {   // 調査用: VCF は調べずに足した後の tz_gate だけ
                    uint64_t h;
                    cnt[c] += fvcf_extra_gate(&b, c, q, &h) > 0;
                } else if (tz_probe_extra(&b, c, q, nodes)) cnt[c]++;
            }
        }
        double dt = now_sec() - t0;
        total += dt;
        // その局面で各色に VCF があるか (手番でない側はパスしたら)
        int hv[3];
        for (int c = BLACK; c <= WHITE; c++) { TssResult tr; hv[c] = fvcf_solve(&b, c, 30, 20000, &tr); }
        int ev = evaluate(&b);
        if (board_side(&b) == WHITE) ev = -ev;
        printf("%d %d %d %d %d %.2f %d %d\t%s\n", cnt[BLACK], cnt[WHITE], f4[BLACK], f4[WHITE], ev, dt * 1000, hv[BLACK], hv[WHITE], tail);
    }
    fprintf(stderr, "time %.3f s\n", total);
    return 0;
}

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
    if (getenv("TSS_ZONE")) g_zone_k = atoi(getenv("TSS_ZONE"));
    if (getenv("TSS_INNER")) g_vcf_inner = atoi(getenv("TSS_INNER"));
    if (getenv("TSS_AND_ZONE")) g_and_zone = atoi(getenv("TSS_AND_ZONE"));
    if (argc < 3) {
        fprintf(stderr, "usage: g2-tss solve|opp <rule> <vcf|vct> [depth] [nodes] | defend <rule> [nodes] | puzzles <file> [nodes]\n");
        return 2;
    }
    if (!strcmp(argv[1], "puzzles")) return cmd_puzzles(argv[2], argc > 3 ? atol(argv[3]) : 2000000);
    Rule rule = parse_rule(argv[2]);
    pattern_init(&rule);
    if (!strcmp(argv[1], "defend")) return cmd_defend(rule, argc > 3 ? atol(argv[3]) : 200000, 0);
    if (!strcmp(argv[1], "vctdefend")) return cmd_defend(rule, argc > 3 ? atol(argv[3]) : 1000000, 1);
    if (getenv("DFPN_VCF")) g_dfpn_vcf_nodes = atol(getenv("DFPN_VCF"));
    if (getenv("DFPN_L2")) g_dfpn_l2 = atoi(getenv("DFPN_L2"));
    if (getenv("DFPN_FLAGS")) g_dfpn_flags = atoi(getenv("DFPN_FLAGS"));
    if (getenv("DFPN_ZONE")) g_dfpn_zone_k = atoi(getenv("DFPN_ZONE"));
    if (!strcmp(argv[1], "dfpn")) return cmd_dfpn(rule, argc > 3 ? atol(argv[3]) : 100000, 0, argc > 4 ? atoi(argv[4]) : 0);
    if (getenv("L3_VCT")) g_l3_vct_nodes = atol(getenv("L3_VCT"));
    if (getenv("L3_QUIET")) g_l3_quiet_nodes = atol(getenv("L3_QUIET"));
    if (getenv("L3_DEPTH")) g_l3_vct_depth = atoi(getenv("L3_DEPTH"));
    { extern int g_l3_or_mode; extern long g_l3_or_nodes;
      if (getenv("L3_ORMODE")) g_l3_or_mode = atoi(getenv("L3_ORMODE"));
      if (getenv("L3_OR")) g_l3_or_nodes = atol(getenv("L3_OR")); }
    { extern int g_l3_def_min; if (getenv("L3_DEF")) g_l3_def_min = atoi(getenv("L3_DEF")); }
    if (getenv("L3_PLY")) g_l3_max_ply = atoi(getenv("L3_PLY"));
    if (getenv("L3_MIN")) g_l3_quiet_min = atoi(getenv("L3_MIN"));
    if (!strcmp(argv[1], "l3count")) return cmd_l3count(rule, argc > 3 ? atol(argv[3]) : 2000, argc > 4 ? atoi(argv[4]) : 0);
    if (!strcmp(argv[1], "zonecheck")) return cmd_zonecheck(rule, argc > 3 ? atol(argv[3]) : 20000);
    if (getenv("TS_VCF")) g_ts_vcf_nodes = atol(getenv("TS_VCF"));
    if (getenv("TS_MIN")) g_ts_threat_min = atoi(getenv("TS_MIN"));
    if (getenv("TS_FLAGS")) g_ts_flags = atoi(getenv("TS_FLAGS"));
    if (getenv("TS_QUIET")) g_ts_quiet_nodes = atol(getenv("TS_QUIET"));
    { extern int g_ts_step, g_ts_start; if (getenv("TS_STEP")) g_ts_step = atoi(getenv("TS_STEP")); if (getenv("TS_START")) g_ts_start = atoi(getenv("TS_START")); }
    if (getenv("TS_LOC")) g_ts_loc = atoi(getenv("TS_LOC"));
    if (getenv("FVCF")) g_fvcf = atoi(getenv("FVCF"));
    if (getenv("TS_LIGHT")) g_ts_light = atoi(getenv("TS_LIGHT"));
    if (!strcmp(argv[1], "p4dump")) return cmd_p4dump(rule);
    if (!strcmp(argv[1], "potential")) return cmd_potential(rule, argc > 3 ? atol(argv[3]) : 30, argc > 4 ? atoi(argv[4]) : P4_FLEX2);
    if (!strcmp(argv[1], "fvcfcheck"))
        return cmd_fvcfcheck(rule, argc > 3 ? atol(argv[3]) : 100000, argc > 4 ? atoi(argv[4]) : 30, argc > 5 ? atoi(argv[5]) : 0);
    if (!strcmp(argv[1], "fvcfrand"))
        return cmd_fvcfrand(rule, argc > 3 ? atoi(argv[3]) : 100, argc > 4 ? (unsigned)atoi(argv[4]) : 1,
                            argc > 5 ? atol(argv[5]) : 100000, argc > 6 ? atoi(argv[6]) : 15);
    if (!strcmp(argv[1], "ts")) return cmd_ts(rule, argc > 3 ? atoi(argv[3]) : 20, argc > 4 ? atoi(argv[4]) : 1000, 0);
    if (!strcmp(argv[1], "tsopp")) return cmd_ts(rule, argc > 3 ? atoi(argv[3]) : 20, argc > 4 ? atoi(argv[4]) : 1000, 1);
    if (!strcmp(argv[1], "tssib")) return cmd_tssib(rule, argc > 3 ? atoi(argv[3]) : 3000);
    if (!strcmp(argv[1], "l3")) return cmd_l3(rule, argc > 3 ? atol(argv[3]) : 100000, 0, argc > 4 ? atoi(argv[4]) : 0);
    if (!strcmp(argv[1], "l3opp")) return cmd_l3(rule, argc > 3 ? atol(argv[3]) : 100000, 1, argc > 4 ? atoi(argv[4]) : 0);
    if (!strcmp(argv[1], "vzcheck")) return cmd_vzcheck(rule, argc > 3 ? atol(argv[3]) : 2000, argc > 4 ? atol(argv[4]) : 200000, argc > 5 ? atoi(argv[5]) : 0);
    if (!strcmp(argv[1], "dfpnopp")) return cmd_dfpn(rule, argc > 3 ? atol(argv[3]) : 100000, 1, argc > 4 ? atoi(argv[4]) : 0);
    if (argc < 4) return 2;
    int mode = !strcmp(argv[3], "vct") ? TSS_VCT : TSS_VCF;
    int depth = argc > 4 ? atoi(argv[4]) : (mode == TSS_VCF ? 30 : 10);
    long nodes = argc > 5 ? atol(argv[5]) : 1000000;
    if (!strcmp(argv[1], "solve")) return cmd_solve(rule, mode, depth, nodes, 0);
    if (!strcmp(argv[1], "opp")) return cmd_solve(rule, mode, depth, nodes, 1);
    fprintf(stderr, "unknown command\n");
    return 2;
}
