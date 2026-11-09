// 脅威圏探索 (Threat Zone): 探索木の中で「手番側がパスしたら相手に VCF があるか」を調べ、
// あれば相手の VCF 手順を崩し得る点 (圏) を返す。圏の外の手 (自分の四を除く) は、打っても同じ手順で
// 相手が勝つ (石が増えて損をしない五目並べの性質による) ので、読まずに捨ててよい。
//
// 圏 = 相手の手順上の点 (攻めの四・止める点・最後の五の点) と、止める点を通る線のうち受け側が
//      そこに置くと三以上になる方向の ±4 マス (受け側がそこに石を足すと止めた石が四になり、手順が崩れる)。
// 見つけた手順は局面のハッシュで覚え、さらに直近の手順を別の局面で「再生」して使い回す
// (兄弟・いとこの節点は遠くの 1 手しか違わないことが多いので、詰み探索をやり直さずに済む)。
#ifndef G2_TZ_H
#define G2_TZ_H

#include "board.h"

#define TZ_MAX_CELLS 160

typedef struct {
    int n;                      // 圏の点の数
    int cells[TZ_MAX_CELLS];
    int plies;                  // 相手の手順の長さ (攻め・受けの手数の合計)
} TzZone;

// 相手 (手番でない側) が VCF を持ち得るかの安い判定 (四を作れる点が 2 つ以上、または四 + 三などの点がある)
static inline int tz_gate(const Board *b, int op) {
    const int16_t *c = b->p4cnt[op];
    return c[P4_BLOCK4] + c[P4_BLOCK4_PLUS] + c[P4_BLOCK4_FLEX3] >= 2 || c[P4_BLOCK4_PLUS] || c[P4_BLOCK4_FLEX3];
}

// 戻り値 0 = 相手に VCF なし (ノード数の上限内で見つからない)、1 = あり (z に圏)、
// 2 = あり、ただし圏を絞れない (止める点が黒の禁じ手になる手順など)
int tz_probe(Board *b, long max_nodes, TzZone *z);

// ルール・盤が変わったとき (search_clear_tt から)
void tz_clear(void);

// 統計 (デバッグ用)
typedef struct {
    long probes, cache_hits, replay_hits, solves, found;
} TzStats;
TzStats tz_stats(void);
void tz_reset_stats(void);

#endif
