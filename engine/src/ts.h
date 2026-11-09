// 評価で手を並べる αβ の脅威探索 (反復深化の AND/OR 探索)。
// 攻め: 四・三・「打った後にパスしたら VCF がある」手 (三でない脅威。直前の攻め側の 2 手の周り)。四を先に、あとは評価の高い順。
// 受け: 攻め側の脅威の圏 (達四の点ごとの手順の圏の共通部分、またはパスしたときの VCF の圏。tz.c) + 自分の四。
//   受け側が先に VCF で勝てれば不詰み。
// 深さ = 攻め側の脅威の手の数 (止める手・五の点の受けは数えない)。証明は脅威圏の健全さの範囲で健全。
#ifndef G2_TS_H
#define G2_TS_H

#include "board.h"

typedef struct {
    int win, move, depth;
    long nodes;
    int aborted;
} TsResult;

int ts_win(Board *b, int attacker, int max_depth, long max_nodes, int max_ms, TsResult *res);
// 本探索の中から呼ぶ版: 置換表を消さない (同じ予算で何度も呼ぶ前提。前の呼び出しの結果を使い回す)
int ts_probe(Board *b, int attacker, int max_depth, long max_nodes, TsResult *res);

// 直前の ts_win / ts_probe の勝ち (同じ盤・同じ攻め側) の証明の圏。受け側の石がこの外に増えても同じ手順で勝つ (脅威圏の健全さの範囲で)。
// 戻り値は個数 (石の点も含む)、-1 = 作れない
int ts_zone(Board *b, int attacker, int *cells, int max);

// ts_zone で作った証明の攻め側の手を覚えて、次の ts_win / ts_probe で先に読む (兄弟の手の確かめを安くする)。消すのは ts_hint_clear
void ts_hint_clear(void);

// 調整用: 中の VCF の確認のノード上限 (200)、脅威の手の pat4 の下限 (P4_BLOCK3)、静かな脅威を探す線の範囲 (3)
extern long g_ts_vcf_nodes, g_ts_quiet_nodes;
extern int g_ts_threat_min, g_ts_loc, g_ts_flags, g_ts_light;

#endif
