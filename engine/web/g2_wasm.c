// ブラウザ (WebAssembly) 用の入口。エンジン本体 (src/) は変えずに、盤の操作と思考だけを関数として出す。
// ビルド: engine/web/build.sh (zig cc で wasm32-wasi)。呼び出しは web/worker.js
#include <string.h>

#include "../src/board.h"
#include "../src/book.h"
#include "../src/fvcf.h"
#include "../src/search.h"
#include "../src/vcf.h"

#define EXPORT(name) __attribute__((export_name(#name)))

static Board g_board;
static Rule g_rule;
static int g_rule_code = -1;
static int g_inited = 0;

// rule: 0=自由 (五以上), 1=ちょうど五, 4=連珠。size: 盤の大きさ (15 など)
EXPORT(g2_new_game) void g2_new_game(int rule, int size) {
    if (!g_inited) {
        board_global_init();
        search_set_tt_size(32);
        g_inited = 1;
    }
    if (rule != g_rule_code) {
        g_rule = rule_from_gomocup(rule);
        pattern_init(&g_rule);
        tss_clear();
        fvcf_clear();
        search_clear_tt();
        g_rule_code = rule;
    }
    if (size < 5 || size > MAX_SIZE) size = 15;
    board_init(&g_board, size, &g_rule);
    search_new_game();
}

// 手番側の石を (x, y) に置く。置けたら 1
EXPORT(g2_play) int g2_play(int x, int y) {
    if (!board_on(&g_board, x, y)) return 0;
    int p = to_pos(x, y);
    if (g_board.cell[p] != EMPTY) return 0;
    board_move(&g_board, p);
    return 1;
}

EXPORT(g2_undo) void g2_undo(void) {
    if (g_board.nmoves > 0) board_undo(&g_board);
}

// 手番が黒で (x, y) が禁じ手なら 1
EXPORT(g2_forbidden) int g2_forbidden(int x, int y) {
    if (!board_on(&g_board, x, y) || board_side(&g_board) != BLACK) return 0;
    return board_is_forbidden(&g_board, to_pos(x, y));
}

// 直前の手で勝負がついたか: 1=黒 2=白 3=引き分け 0=続行 (types.h の BLACK/WHITE に合わせて返す)
EXPORT(g2_result) int g2_result(void) {
    int r = board_last_move_wins(&g_board);
    if (r == BLACK) return 1;
    if (r == WHITE) return 2;
    return r == 3 ? 3 : 0;
}

// ビルド元のコミット (build.sh が -DG2_COMMIT=... で埋め込む)。UI が wasm から読んで表示する
#ifndef G2_COMMIT
#define G2_COMMIT "unknown"
#endif
EXPORT(g2_version) const char *g2_version(void) { return "G2 " G2_COMMIT; }

// 直前の思考の情報 (UI の表示用)
enum { INFO_SCORE, INFO_DEPTH, INFO_KNODES, INFO_MS, INFO_BOOK, INFO_SELDEPTH, INFO_PVLEN, INFO_NODES, INFO_N };
static int g_info[INFO_N];
static SearchResult g_res;

// 手番側の手を考える (盤には置かない)。戻り値 y * 32 + x、打てる手がなければ -1。
// 時間の配分は think.c の plan_time と同じ (持ち時間から余裕を引き、soft_pct % で次の反復を止める)
EXPORT(g2_think) int g2_think(int time_ms) {
    memset(g_info, 0, sizeof g_info);
    memset(&g_res, 0, sizeof g_res);
    g_res.reason = "book";
    int bm;
    if (book_probe(&g_board, &bm)) {
        g_info[INFO_BOOK] = 1;
        return pos_y(bm) * 32 + pos_x(bm);
    }
    int margin = time_ms / 20 + 30;
    if (margin > 300) margin = 300;
    int hard = time_ms - margin;
    if (hard < 20) hard = 20;
    SearchLimits sl = {0};
    sl.hard_ms = hard;
    sl.soft_ms = hard * search_soft_pct() / 100;
    search_root(&g_board, &sl, &g_res);
    g_info[INFO_SCORE] = g_res.score;
    g_info[INFO_DEPTH] = g_res.depth;
    g_info[INFO_KNODES] = (int)(g_res.nodes / 1000);
    g_info[INFO_MS] = g_res.ms;
    g_info[INFO_SELDEPTH] = g_res.seldepth;
    g_info[INFO_PVLEN] = g_res.pvlen;
    g_info[INFO_NODES] = (int)g_res.nodes;
    if (g_res.move < 0) return -1;
    return pos_y(g_res.move) * 32 + pos_x(g_res.move);
}

EXPORT(g2_info) int g2_info(int i) { return (i >= 0 && i < INFO_N) ? g_info[i] : 0; }

// 読み筋の i 手目 (y * 32 + x)。範囲外は -1
EXPORT(g2_pv) int g2_pv(int i) {
    if (i < 0 || i >= g_res.pvlen || g_res.pv[i] < 0) return -1;
    return pos_y(g_res.pv[i]) * 32 + pos_x(g_res.pv[i]);
}

// 手を決めた理由 ("search", "vcf", "five", "book" など。search.c の SearchResult.reason)
EXPORT(g2_reason) const char *g2_reason(void) { return g_res.reason ? g_res.reason : ""; }
