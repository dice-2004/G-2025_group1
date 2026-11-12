// 評価関数の表
//
// 空点 q に色 c が打ったときの 4 方向パターンの組 (pattern_index) → 点数 g_cell_value[c][idx]。
// 盤面の評価は「空点の点数の和」(Board.vsum を差分更新) の手番側 − 相手側。
// 点数は線パターンごとの重み + 4 方向の組み合わせ (Pattern4) ごとの加点で作る。
#ifndef G2_EVAL_H
#define G2_EVAL_H

#include "pattern.h"

extern int16_t g_cell_value[3][PIDX_COUNT];

// 盤面の脅威の種類 (その色が次の 1 手で作れる形)
enum { THREAT_B4F3 = 0, THREAT_F3X2, THREAT_F3, THREAT_B4PLUS, THREAT_B4X2, THREAT_B3PLUS, THREAT_F2X2, THREAT_F3MANY, THREAT_COUNT };

// 空点の Pattern4 の数 → 脅威のビット集合 (THREAT_* の位置)
static inline int eval_threat_bits(const int16_t *cnt) {
    int f3 = cnt[P4_FLEX3] + cnt[P4_FLEX3_PLUS];
    return (cnt[P4_BLOCK4_FLEX3] > 0) << THREAT_B4F3 | (cnt[P4_FLEX3_2X] > 0) << THREAT_F3X2 |
           (f3 > 0) << THREAT_F3 | (cnt[P4_BLOCK4_PLUS] > 0) << THREAT_B4PLUS |
           (cnt[P4_BLOCK4] + cnt[P4_BLOCK4_PLUS] + cnt[P4_BLOCK4_FLEX3] >= 2) << THREAT_B4X2 |
           (cnt[P4_BLOCK3_PLUS] > 0) << THREAT_B3PLUS | (cnt[P4_FLEX2_2X] > 0) << THREAT_F2X2 |
           (f3 >= 4) << THREAT_F3MANY;   // 活三を作れる点が多い (2 本以上の二)
}

// 色 c の一番速い脅威の段階 (空点の Pattern4 の数から): 0 = 五の点、1 = 達四の点 (活三がある)、2 = 四三・三三の点、
// 3 = 四の点と活三の点の両方、4 = 活三の点、5 = 四の点、6 = なし (EVAL_RACE_LEVELS 段階)
#define EVAL_RACE_LEVELS 7
static inline int eval_threat_level(const int16_t *c) {
    if (c[P4_FIVE]) return 0;
    if (c[P4_FLEX4]) return 1;
    if (c[P4_BLOCK4_FLEX3] || c[P4_FLEX3_2X]) return 2;
    int f4 = c[P4_BLOCK4] + c[P4_BLOCK4_PLUS], f3 = c[P4_FLEX3] + c[P4_FLEX3_PLUS];
    if (f4 && f3) return 3;
    if (f3) return 4;
    if (f4) return 5;
    return 6;
}

// 調整できる重み (eval.c の既定値。tune 用に書き換え可能)
typedef struct {
    int line[P_COUNT];          // 線パターンごと
    int combo[P4_COUNT];        // Pattern4 ごとの加点
    int opp_scale;              // 相手側の和に掛ける割合 (/64)
    int tempo;                  // 手番の加点
    // 脅威の加点 [手番が黒=0/白=1][手番側=0/相手側=1][脅威の種類 THREAT_*]。空点の形の数 (Board.p4cnt) から O(1) で出す
    int threat[2][2][THREAT_COUNT];
    int use_table;      // 1 = eval_table.c の補正を使う (連珠のみ)
    // 脅威の速さの比べ (race、1 = 使う): 手番側の段階 m、相手の段階 o (eval_threat_level) で、
    // o >= m (手番側の脅威が同じか速い。手番側が先に打てる) なら + race_own[m]、o < m (相手が速い) なら − race_opp[o]
    int race;
    // 三・四を作れる点の数 (上限 8) の重み: 自分の三の点 / 相手の三の点 / 自分の四の点 / 相手の四の点 (足す値、相手側は引く値)。
    // Rapfi の評価との差を見ると、G2 は相手の三の点 (ふさぐべき伸びる方向) を軽く、自分の四の点 (止め三) を重く見ている
    int cnt_f3_me, cnt_f3_op, cnt_b4_me, cnt_b4_op;
    // フォーク積: 空点の値に、方向の違う 2 本の線の「静かな形の強さ」の積の和 × cross / 16 を足す (0 = 使わない)。
    // 勝ちは 1 点で 2 本の線が同時に脅威になること (四三・三三) なので、まだ三・四でない線どうしの交わりを掛け算で数える
    int cross;
    int race_own[EVAL_RACE_LEVELS], race_opp[EVAL_RACE_LEVELS];
} EvalParams;

// 空点の値の補正 (tests/tune_table.py で作る eval_table.c)。pidx は 4 方向のパターンを小さい順に並べた組
typedef struct {
    int color, pidx, delta;
} EvalTableEntry;
extern const EvalTableEntry g_eval_table[];
extern const int g_eval_table_count;
extern const int g_eval_table_rule;

extern EvalParams g_eval_params;

void eval_init(const Rule *rule);   // pattern_init から呼ばれる。g_eval_params から表を作る

// 点数の計算に使う Pattern4 (黒の禁じ手の補正込み)。-1 = 価値なし (黒の四四・長連の点)
int eval_cell_class(const Rule *rule, int color, const uint8_t pats[4]);

#endif
