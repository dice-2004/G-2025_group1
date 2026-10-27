// 指し手選択: 持ち時間から 1 手の目安と上限を決めて本探索 (search.c) を呼ぶ
#include "think.h"

#include <stdio.h>

#include "book.h"
#include "search.h"

// 1 手の上限 (hard) と、次の反復を始めない目安 (soft) を決める
static void plan_time(const Board *b, const ThinkLimits *lim, SearchLimits *sl) {
    int turn = lim->time_ms > 0 ? lim->time_ms : 5000;
    // 通信や後処理の余裕を引く
    int margin = turn / 20 + 30;
    if (margin > 300) margin = 300;
    int hard = turn - margin;
    if (lim->time_left_ms > 0) {
        // 残り時間を残りの手数で割る (盤が埋まるほど残り手数は少ない)
        int empties = b->size * b->size - b->nmoves;
        int moves_left = empties / 6;
        if (moves_left < 8) moves_left = 8;
        if (moves_left > 40) moves_left = 40;
        int share = lim->time_left_ms / moves_left;
        int cap = lim->time_left_ms / 4 - 30;
        if (hard > share * 2) hard = share * 2;
        if (hard > cap) hard = cap;
    }
    if (hard < 20) hard = 20;
    sl->hard_ms = hard;
    // 反復が 1 段深くなると数倍かかるので、上限の soft_pct % を過ぎたら次を始めない
    sl->soft_ms = hard * search_soft_pct() / 100;
}

int think_move(Board *b, const ThinkLimits *lim) {
    int bm;
    if (book_probe(b, &bm)) {
        if (lim->verbose) fprintf(stderr, "move %d,%d (book)\n", pos_x(bm), pos_y(bm));
        return bm;
    }
    SearchLimits sl = {0};
    plan_time(b, lim, &sl);
    sl.max_depth = lim->max_depth;
    sl.max_nodes = lim->max_nodes;
    sl.verbose = lim->verbose;
    SearchResult res;
    search_root(b, &sl, &res);
    if (lim->verbose)
        fprintf(stderr, "move %d,%d score %d depth %d nodes %ld time %d ms (%s)\n", pos_x(res.move), pos_y(res.move),
                res.score, res.depth, res.nodes, res.ms, res.reason);
    return res.move;
}
