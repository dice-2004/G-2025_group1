// VCF (四の連続) / VCT (四と活三の連続) による詰み探索
//
// 攻め側が「相手に受けの選択肢がほぼない脅威」だけを打ち続けて勝てるかを調べる。
//   VCF: 攻めは四だけ。受けは五にする点を止める 1 通り
//   VCT: 攻めは四か活三。活三の受けは「三の線を止める手」と受け側の四 (逆四) を全部試す
// 連珠の禁じ手は両側とも考慮する (黒は禁じ手で四・三を作れない / 白の四の止め点が黒の禁じ手なら止められない)。
// 「勝ち」は健全: 深さやノード数で打ち切ったときは勝ちと言わない。
#ifndef G2_VCF_H
#define G2_VCF_H

#include "board.h"

enum { TSS_VCF = 0, TSS_VCT = 1 };

#define TSS_MAX_LINE 128

typedef struct {
    int win;                    // 1 = 攻め側の勝ちを証明
    int move;                   // 勝ちの初手 (なければ -1)
    int line[TSS_MAX_LINE];     // 攻め・受けが交互の手順 (攻めから。VCT の受けは代表 1 手)
    int nline;
    long nodes;                 // 調べた局面数
    int aborted;                // ノード数の上限で打ち切った (勝ちが見つからなかったとき意味がある)
} TssResult;

// 手番側が VCF / VCT で勝てるか。勝てれば 1 を返し *win_move に初手 (NULL 可)。盤は元に戻す。
// max_depth は攻め側の手数の上限 (VCF は 30 前後、VCT は 8〜12 程度)、max_nodes は局面数の上限
int vcf_solve(Board *b, int max_depth, long max_nodes, int *win_move);
int vct_solve(Board *b, int max_depth, long max_nodes, int *win_move);

// 一般形: attacker が手番でなくてもよい (= 手番側がパスしたら attacker が詰むか)。
// mode は TSS_VCF / TSS_VCT。VCT は深さ 1 からの反復深化で最短の勝ちを探す。res は NULL 可
int tss_solve(Board *b, int attacker, int mode, int max_depth, long max_nodes, TssResult *res);

// 相手 (手番でない側) に VCF があるとき、それを崩す手番側の手を moves に入れて個数を返す。
// 自分の四 (手番を取る手) は無検証で含め、それ以外は打った後に相手の VCF が消えることを確かめた手だけ。
// 相手に VCF がなければ -1。moves は MAX_MOVES 個分の領域
int vcf_defenses(Board *b, long max_nodes, int *moves);

// 相手 (手番でない側) に VCF か VCT があるとき、それを崩し得る手番側の手を moves に入れて個数を返す。
// 相手に VCF があれば vcf_defenses と同じ。VCT だけなら、石の周りの空点などを 1 手ずつ打って相手の VCT が
// 消えるか (または打ち切りで不明か) を確かめた手と、自分の四 (無検証) を返す。返さなかった手は
// 「打っても相手に VCT がある (= 負け)」と証明済み。どちらもなければ -1。
// max_nodes の 1/4 で相手の詰みを探し、残りを候補の検証に等分する
int vct_defenses(Board *b, int max_depth, long max_nodes, int *moves);

// 調整用: VCT の三を探す範囲 (探索中の攻め側の直近何手の周りか、0 = 盤全体。既定 2) と、
// VCT の各局面で先に試す VCF の深さ (既定 24)
extern int g_zone_k;
extern int g_vcf_inner;

// 置換表を空にする (ルールや盤サイズを変えたとき。pattern_init の後に呼ぶ)
void tss_clear(void);

#endif
