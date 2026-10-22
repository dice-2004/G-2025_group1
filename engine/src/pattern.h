// 線パターン表
//
// あるマスに石を置いたとき、1 方向の直線上でどんな形になるかを、
// 中心から ±5 マス (計 10 マス) の並びから表引きで求める。
// 並びは 1 マス 2 ビット (0=空 1=黒 2=白 3=盤外) で 20 ビットのキーにする。
#ifndef G2_PATTERN_H
#define G2_PATTERN_H

#include "types.h"

// 1 方向のパターン (弱い順)。その色が中心に打った場合の形
enum Pattern {
    P_DEAD = 0, // 五になる見込みなし
    P_OL,       // 打つと長連 (黒の長連が禁じ手のときだけ)
    P_B1,       // 止め一
    P_F1,       // 活一
    P_B2,       // 止め二 (止め三にできる)
    P_F2,       // 活二 (活三にできる)
    P_B3,       // 止め三 (四にできる)
    P_F3,       // 活三 (達四にできる)
    P_B4,       // 四 (五にする点が 1 つ)
    P_F4,       // 達四 (_XXXX_、止められない)
    P_D4,       // 一直線上の四四 (X.XXX.X など、五にする点が 2 つあるが達四ではない)
    P_F5,       // 五
    P_COUNT
};

// 4 方向を合わせたマスの評価 (Rapfi 等と同様の分類、強い順)
enum Pattern4 {
    P4_NONE = 0,
    P4_FLEX2,           // 活二
    P4_BLOCK3,          // 止め三
    P4_FLEX2_2X,        // 活二が 2 つ
    P4_BLOCK3_PLUS,     // 止め三 + 活二以上
    P4_FLEX3,           // 活三
    P4_FLEX3_PLUS,      // 活三 + 止め三/活二
    P4_FLEX3_2X,        // 三三
    P4_BLOCK4,          // 四
    P4_BLOCK4_PLUS,     // 四 + 止め三/活二
    P4_BLOCK4_FLEX3,    // 四三
    P4_FLEX4,           // 達四 または 四四 (止められない)
    P4_FIVE,            // 五
    P4_COUNT
};

#define KEY_BITS 20
#define KEY_COUNT (1u << KEY_BITS)

// 表を作る (ルールが変わったら作り直す)。0.数秒かかる
void pattern_init(const Rule *rule);

// key: 中心以外の 10 マスの並び。color の石を中心に置いたときのパターン
extern uint8_t g_pattern_table[3][KEY_COUNT];   // [color][key]、color は 1,2 のみ有効
static inline int pattern_of(int color, uint32_t key) { return g_pattern_table[color][key]; }

// 中心から offset (-5..-1, 1..5) のマスのキー内ビット位置
static inline int key_slot(int offset) { return offset < 0 ? offset + 5 : offset + 4; }

int combine_pattern4(const uint8_t pats[4]);
const char *pattern_name(int p);
const char *pattern4_name(int p4);

#endif
