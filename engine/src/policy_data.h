// 探索の手の並べ方の重み (engine/tests/tune_policy.py が policy_data.c に書き出す)。
// 候補手の点 = 今の点 (攻め + 守り) * base_scale/100 + 自分の 4 方向の線パターンの点の和 + 自分の Pattern4 の点
//            + 相手の (同) + 直前の相手の手との距離の点 + 自分の前の手との距離の点   (1 logit = 256)
#ifndef G2_POLICY_DATA_H
#define G2_POLICY_DATA_H

extern const int g_policy_line_me[2][12];   // [手番の色 黒=0 白=1][線パターン]
extern const int g_policy_line_op[2][12];
extern const int g_policy_p4_me[2][13];     // [手番の色][Pattern4]
extern const int g_policy_p4_op[2][13];
extern const int g_policy_dist_last[7];     // 直前の相手の手との距離 (チェビシェフ、6 以上は 6)
extern const int g_policy_dist_mine[7];     // 自分の前の手との距離
extern const int g_policy_base_scale;       // 今の候補手の点 1 あたりの係数 (/100)

#endif
