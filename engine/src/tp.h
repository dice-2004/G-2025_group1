// 脅威の潜在力 (threat potential): 局面の「打ってパスしたら VCF がある点」(λ2 の脅威の点) の数を両方の色について数え、
// 評価の補正にする。パターンの評価には見えない「次に脅威を作れる力」(主導権) を、詰み探索 (tz.c / fvcf.c) で測る。
//
// 本探索では、根から sp.tp_ply 手目の局面でだけ数えて (局面のハッシュで覚える)、その局面の値に足す
// (下の探索の窓を補正の分だけずらすので、下の探索はそのまま。search.c の pvs の入口のフック)。
// 補正 = 手番側の点の数 × g_tp_own − 相手の点の数 × g_tp_opp (それぞれ g_tp_cap 個まで)
#ifndef G2_TP_H
#define G2_TP_H

#include "board.h"

// color の λ2 の脅威の点の数 (空点で石の近く、pat4 が lo 以上 P4_FLEX3 未満、打てる点で、打った後にパスしたら color に VCF)。
// cap 個で打ち切る
int tp_count(Board *b, int color, int lo, long nodes, int cap);

// 手番側から見た補正 (覚えた値を使う)
int tp_correction(Board *b);

// 手の並べ方 (search.c の pvs のフック): tp_prepare(b) で手番側と相手の λ2 の脅威の点を (覚えた値から) 用意し、
// tp_mark(q) が 1 = 手番側の点、2 = 相手の点 (両方なら 3) を返す。tp_prepare は点の総数を返す
int tp_prepare(Board *b);
int tp_mark(int q);

void tp_clear(void);

extern int g_tp_own, g_tp_opp, g_tp_lo, g_tp_cap;
extern long g_tp_nodes;
extern _Thread_local long g_tp_calls, g_tp_hits;

#endif
