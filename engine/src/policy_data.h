// 探索の手の並べ方の表 (engine/tests/tune_policy.py が policy_data.c に書き出す)
#ifndef G2_POLICY_DATA_H
#define G2_POLICY_DATA_H

typedef struct {
    int color;      // 手番の色 (BLACK/WHITE)
    int role;       // 0 = 自分のパターンの組、1 = 相手のパターンの組
    int spidx;      // 4 方向のパターンを昇順に並べた組の pattern_index
    int w;          // 点 (tune_policy.py の --scale が 1 logit)
} PolicyEntry;

extern const PolicyEntry g_policy_entries[];
extern const int g_policy_count;
extern const int g_policy_dist_last[7];   // 直前の相手の手との距離 (チェビシェフ、6 以上は 6)
extern const int g_policy_dist_mine[7];   // 自分の前の手との距離
extern const int g_policy_base_scale;     // 今の候補手の点 (攻め + 守り) 1 あたりの係数 (/100)

#endif
