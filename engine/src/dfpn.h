// 証明数探索 (df-pn) による VCT。受けの候補は脅威圏 (tz.c) から作る。
//
// 攻め側の局面 (OR): 五 → 勝ち、今 VCF がある → 勝ち (tz_probe_att、手順を覚えて再生)。
//   手は四と活三 (三は探索中の攻め側の直前の 2 手の周り)。受け側の四には止める手だけ。
// 受け側の局面 (AND): 攻め側の五の点が 2 つ → 勝ち、1 つ → 止めるしかない。
//   攻め側に達四の点があれば、点 t ごとの「t に打って勝つ」手順の圏の共通部分、なければパスしたときの攻め側の
//   VCF の圏 (どちらも外の手は同じ手順で負ける)。これと受け側の四が受けの候補。受け側が先に VCF で勝てれば不詰み。
// 深さ優先の証明数探索 (Nagai の df-pn、閾値に 1+ε)。置換表に証明数・反証数を覚える。
#ifndef G2_DFPN_H
#define G2_DFPN_H

#include "board.h"

typedef struct {
    int win;
    int move;       // 勝ちの初手
    long nodes;     // 展開した局面数
    int aborted;    // ノード数の上限で打ち切った
} DfpnResult;

// attacker を手番として VCT を調べる (attacker が手番でなくてもよい: パスしたら)。勝てれば 1。
// max_nodes / max_ms (0 = 時間の制限なし) のどちらかに達したら打ち切る
int dfpn_vct(Board *b, int attacker, long max_nodes, int max_ms, DfpnResult *res);

// 直前の dfpn_vct の証明 (同じ盤・同じ attacker で勝ち) の圏: 受け側の石がこの外に増えても勝ちは (ほぼ) 変わらない。
// cells に空点も石の点も入れる。戻り値は個数、-1 = 絞れない (証明木が大きすぎる・圏を絞れない VCF がある)
int dfpn_vct_zone(Board *b, int attacker, int *cells, int max);

// 調整用: VCF の確認 1 回のノード数の上限 (既定 100、INFO p_dfpn_vcf)、三を探す範囲 (攻め側の直前の何手の周りか、0 = 全体、既定 2)
extern long g_dfpn_vcf_nodes;
extern int g_dfpn_zone_k;
extern int g_dfpn_flags;
extern int g_dfpn_l2;   // 1 = 三でない「パスしたら VCF」の脅威も攻めに入れる (試し。詰みの物差しで悪化、既定 0)

#endif
