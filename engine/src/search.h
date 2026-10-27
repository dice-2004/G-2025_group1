// 本探索: 反復深化 + PVS (αβ) + 置換表 + 手の並べ替え
#ifndef G2_SEARCH_H
#define G2_SEARCH_H

#include "board.h"

#define SCORE_WIN 30000                 // 「ply 手目に五を作る」側の点は SCORE_WIN - ply
#define SCORE_WIN_MIN (SCORE_WIN - 1000) // これより絶対値が大きければ詰み (勝ち/負けが確定)
#define SCORE_INF 32000
#define MAX_PLY 128

typedef struct {
    int soft_ms;        // これを過ぎたら次の反復を始めない
    int hard_ms;        // 探索を打ち切る
    int max_depth;      // 0 = 制限なし
    long max_nodes;     // 0 = 制限なし
    int verbose;        // 1 以上で反復ごとの情報を stderr に出す
} SearchLimits;

typedef struct {
    int move;           // 選んだ手 (盤上の pos)。-1 = 打てる手なし
    int score;          // 手番側から見た評価
    int depth;          // 完了した反復の深さ
    int seldepth;
    long nodes;
    int ms;
    int pv[MAX_PLY];
    int pvlen;
    const char *reason; // 手を決めた理由 ("search", "vcf", "five", ...)
} SearchResult;

void search_set_tt_size(int mb);   // 置換表の大きさ (既定 32MB)
void search_new_game(void);        // 履歴をクリア (置換表は残す)
void search_clear_tt(void);        // 置換表をクリア (ルール・評価の重みが変わったとき)
void search_root(Board *b, const SearchLimits *lim, SearchResult *res);

// 調整用の値を名前で設定 (qdepth, q_min_p4, soft_pct, opp_scale, tempo, line<N>, combo<N>)。
// 評価の重みを変えたら pattern_init (eval_init) で表を作り直すこと。知らない名前なら 0
int search_set_param(const char *name, long value);
int search_soft_pct(void);

// 手番側から見た静的評価
int evaluate(const Board *b);

#endif
