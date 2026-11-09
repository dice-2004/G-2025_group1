// λ3 探索 (証明数探索)。脅威 = 「打って、受け側がパスしたら攻め側に VCT (dfpn_vct) がある」手。
// 四・三 (λ1・λ2) も同じ定義に入る。VCT では見えない静かな手 (二を二つ作る手など) を含む勝ちを見つける。
//
// 攻め側の局面 (OR): 今 VCT がある → 勝ち。手は「pat4 が活二二つ以上」の点のうち、打った後に VCT が成り立つもの
//   (静かな手は攻め側の直前の 2 手の周り 3 マスまで)。受け側の五の点は止めるしかない。
// 受け側の局面 (AND): 攻め側の「パスしたら VCT」の証明の圏 (dfpn_vct_zone) の空点 + 受け側の三・四が受けの候補。
//   受け側が先に VCF で勝てれば不詰み。
// 証明の圏の外の受けは同じ VCT で負ける (受け側の新しい四を除く) ので、証明はほぼ健全。反証は候補の制限で甘い。
#ifndef G2_L3_H
#define G2_L3_H

#include "board.h"

typedef struct {
    int win;
    int move;
    long nodes;       // λ3 の局面数
    long vct_calls;   // 中で呼んだ dfpn_vct の回数
    int aborted;
} L3Result;

// attacker を手番として (手番でなくても: パスしたら) λ3 の勝ちを調べる
int l3_win(Board *b, int attacker, long max_nodes, int max_ms, L3Result *res);

// 調整用: 局面ごとの VCT の確認のノード上限 (既定 3000)、静かな手の確認の上限 (既定 300)、静かな手の pat4 の下限
extern long g_l3_vct_nodes, g_l3_quiet_nodes;
extern int g_l3_quiet_min, g_l3_vct_depth;

#endif
