// VCF 専用の軽い盤による VCF (tss_solve の TSS_VCF と同じ探索を速く)
//
// 盤 b は読むだけ。VCF の手は自前の重ね盤 (触ったマスだけ盤から写して差分更新する) に打ち、
// 攻め側の線パターンと「四になる点」の集合だけを保つ (受け側のパターンは要る時に表から引く)。
// 手の順・置換表・ノードの数え方は vcf.c と同じなので、置換表の状態が同じなら
// 結果 (勝ち・初手・手順・ノード数・打ち切り) が tss_solve(b, att, TSS_VCF, ...) と一致する (tss_test fvcfcheck で確かめる)。
// 連珠の禁じ手は board_is_forbidden と同じ手順 (偽の三を再帰で除く) を重ね盤の上で行う。
#ifndef G2_FVCF_H
#define G2_FVCF_H

#include "tz.h"
#include "vcf.h"

// tss_solve(b, attacker, TSS_VCF, max_depth, max_nodes, res) と同じ。res は NULL 可
int fvcf_solve(Board *b, int attacker, int max_depth, long max_nodes, TssResult *res);

// tz.c の手順の再生 (att の VCF 手順 line を今の盤で確かめて圏を作る) と同じ。戻り値も同じ
int fvcf_replay(Board *b, int att, const int16_t *line, int n, TzZone *z);

// 盤に打たずに、盤 b に att の石 extra を足した局面を調べる (ts の静かな手の確認用)。
// fvcf_extra_gate: 戻り値は tz_gate(足した盤, att)、*hash は足した盤のハッシュ。-1 = この形では使えない
//   (五の点がある、extra が四の点など。呼び出し側は盤に打つ版を使う)。
// fvcf_extra_replay_begin の後の fvcf_extra_replay は足した盤での fvcf_replay (圏は作らない) の戻り値と同じ。
// fvcf_extra_solve は足した盤での fvcf_solve と同じ (gate が -1 でないときだけ呼ぶ)
int fvcf_extra_gate(Board *b, int att, int extra, uint64_t *hash);
void fvcf_extra_replay_begin(Board *b, int att, int extra);
int fvcf_extra_replay(const int16_t *line, int n);
int fvcf_extra_solve(Board *b, int att, int extra, int max_depth, long max_nodes, TssResult *res);

// 置換表を空にする (tss_clear と同じ時に呼ぶ)
void fvcf_clear(void);
// 呼び出し元のスレッドの番号 (tss_set_thread と同じ番号)
void fvcf_set_thread(int id);

// 1 (既定) = 脅威圏 (tz.c)・ルートと末端の VCF にこちらを使う (INFO p_fvcf 0 で vcf.c)
extern int g_fvcf;

// 統計 (テスト用): 禁じ手と判定した回数 [0]=長連 [1]=四四 [2]=三三
extern _Thread_local long g_fvcf_forbid[3];

#endif
