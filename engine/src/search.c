// 本探索
//
// - 反復深化 + PVS (null window で調べて、良ければ全幅で再探索)
// - 置換表 (Zobrist ハッシュ、2 エントリのバケット、深さ優先 + 世代で置き換え)
// - 強制手: 相手に五の点があれば止める手だけ (深さを減らさない = 四への延長)。
//   相手に達四の点 (活三など) があれば、防ぎ手と自分の四だけ
// - 手の並べ替え: 置換表の手 → パターン点 (攻め + 守り) + killer + history
// - 遅い手の深さ削減 (LMR) と浅い所での手数打ち切り (戦術的でない手のみ)
#include "search.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "eval.h"
#include "dfpn.h"
#include "l3.h"
#include "rz.h"
#include "ts.h"
#include "tp.h"
#include "fvcf.h"
#include "policy_data.h"
#include "tz.h"
#include "vcf.h"

// ---------------------------------------------------------------- 置換表

// 複数スレッドで共有する。ロックは使わず、key には「局面のハッシュ ^ data」を入れて、
// 書き込みが混ざったエントリは key が合わなくなって捨てられるようにする
typedef struct {
    uint64_t key;
    uint64_t data;  // value 16 | move 16 | depth 8 | flag 8 | gen 8
} TTEntry;

typedef struct {
    int value, move, depth, flag;
} TTData;

enum { TT_NONE = 0, TT_UPPER = 1, TT_LOWER = 2, TT_EXACT = 3 };

static TTEntry *tt;
static size_t tt_mask;      // バケット数 - 1 (1 バケット = 2 エントリ)
static uint8_t tt_gen;
static int tt_mb = 32;
static int tt_used;         // 確保してから探索に使ったか

void search_set_tt_size(int mb) {
    if (mb < 1) mb = 1;
    if (tt && mb == tt_mb) return;
    tt_mb = mb;
    free(tt);
    tt = NULL;
    tt_used = 0;
}

static void tt_alloc(void) {
    if (tt) return;
    size_t n = 1;
    while (n * 2 * sizeof(TTEntry) * 2 <= (size_t)tt_mb << 20) n *= 2;
    tt = calloc(n * 2, sizeof(TTEntry));
    tt_mask = n - 1;
}

static int value_to_tt(int v, int ply) {
    if (v > SCORE_WIN_MIN) return v + ply;
    if (v < -SCORE_WIN_MIN) return v - ply;
    return v;
}

static int value_from_tt(int v, int ply) {
    if (v > SCORE_WIN_MIN) return v - ply;
    if (v < -SCORE_WIN_MIN) return v + ply;
    return v;
}

static inline int d_value(uint64_t d) { return (int16_t)(d & 0xFFFF); }
static inline int d_move(uint64_t d) { return (int16_t)(d >> 16 & 0xFFFF); }
static inline int d_depth(uint64_t d) { return (int8_t)(d >> 32 & 0xFF); }
static inline int d_flag(uint64_t d) { return (int)(d >> 40 & 0xFF); }
static inline int d_gen(uint64_t d) { return (int)(d >> 48 & 0xFF); }

static inline uint64_t ld(const uint64_t *p) { return __atomic_load_n(p, __ATOMIC_RELAXED); }
static inline void st(uint64_t *p, uint64_t v) { __atomic_store_n(p, v, __ATOMIC_RELAXED); }

static int tt_probe(uint64_t key, TTData *out) {
    TTEntry *e = &tt[(key & tt_mask) * 2];
    for (int i = 0; i < 2; i++) {
        uint64_t d = ld(&e[i].data);
        if ((ld(&e[i].key) ^ d) == key && d_flag(d)) {
            out->value = d_value(d);
            out->move = d_move(d);
            out->depth = d_depth(d);
            out->flag = d_flag(d);
            return 1;
        }
    }
    return 0;
}

static void tt_store(uint64_t key, int value, int move, int depth, int flag, int ply) {
    TTEntry *e = &tt[(key & tt_mask) * 2];
    uint64_t d0 = ld(&e[0].data), d1 = ld(&e[1].data);
    int m0 = (ld(&e[0].key) ^ d0) == key, m1 = (ld(&e[1].key) ^ d1) == key;
    int slot;
    uint64_t old;
    if (m0) slot = 0, old = d0;
    else if (m1) slot = 1, old = d1;
    else {
        // 古い世代か浅いほうを置き換える
        int s0 = d_depth(d0) - (d_gen(d0) != tt_gen) * 64;
        int s1 = d_depth(d1) - (d_gen(d1) != tt_gen) * 64;
        slot = (s0 <= s1) ? 0 : 1;
        old = 0;
    }
    int same = m0 || m1;
    if (same && move < 0) move = d_move(old);   // 手の情報は残す
    if (same && d_gen(old) == tt_gen && depth < d_depth(old) - 2 && flag != TT_EXACT) return;
    if (depth < -100) depth = -100;
    if (depth > 100) depth = 100;
    uint64_t d = (uint64_t)(uint16_t)value_to_tt(value, ply) | (uint64_t)(uint16_t)move << 16 |
                 (uint64_t)(uint8_t)depth << 32 | (uint64_t)(uint8_t)flag << 40 | (uint64_t)tt_gen << 48;
    st(&e[slot].key, key ^ d);
    st(&e[slot].data, d);
}

// ---------------------------------------------------------------- 共通

#define MAX_THREADS 16

typedef struct {
    Board *b;
    long nodes;
    long max_nodes;
    int stop;
    int id;             // 0 = 主スレッド (時間を見る・手を決める)
    double t0;
    int hard_ms;
    int seldepth;
    int root_depth;
    int killer[MAX_PLY][2];
    int pv[MAX_PLY][MAX_PLY];
    int pvlen[MAX_PLY];
    int history[3][NCELL];  // 前の手の情報を少し残す (search_root で 1/4 に)
    uint32_t tzmark[NCELL]; // 脅威圏の点の印 (== tzgen なら圏内)。gen_moves の間だけ使う
    uint32_t tzgen;
    int seval[MAX_PLY];     // 節点の静的評価 (手の数の枝刈り・futility の「評価が良くなっているか」用)
    uint8_t xhalf[MAX_PLY]; // ext_three == 2 の半手の延長の繰り越し (その手数の節点に渡る)
    int drop4;          // gen_moves: 相手の活三を受ける節点で、受けにならない自分のただの四を除く
} Ctx;

static Ctx ctxs[MAX_THREADS];
static Board boards[MAX_THREADS];   // 補助スレッド用の盤のコピー
static int n_threads = 1;
static int g_stop;                  // 主スレッドが探索を終えたら 1 (補助スレッドを止める)

void search_set_threads(int n) {
    n_threads = n < 1 ? 1 : n > MAX_THREADS ? MAX_THREADS : n;
}

// 新しい対局: 置換表は局面のハッシュで引くので前の対局の内容も正しいまま使える (消さない)。
// 全体を消すと 128MB の確保直後は数百 ms かかる (ページの割り当て) ので、ルールや評価が変わった時だけ消す
void search_new_game(void) {
    tt_alloc();
    for (int i = 0; i < MAX_THREADS; i++) memset(ctxs[i].history, 0, sizeof ctxs[i].history);
}

void search_clear_tt(void) {
    if (tt && tt_used) memset(tt, 0, (tt_mask + 1) * 2 * sizeof(TTEntry));   // 確保したばかりなら 0 のまま
    tt_used = 0;
    tt_gen = 0;
    tz_clear();
    tp_clear();
}

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec * 1e-6;
}

static inline int restricted(const Board *b, int color) {
    return color == BLACK && rule_has_forbidden(&b->rule);
}

static inline int legal(Board *b, int color, int q) {
    return !restricted(b, color) || !board_is_forbidden(b, q);
}

// 盤上の空点で color のパターンが p4 の点を 1 つ探す (なければ -1)
static int find_cell(const Board *b, int color, int p4) {
    for (int y = 0; y < b->size; y++) {
        int q = to_pos(0, y);
        for (int x = 0; x < b->size; x++, q++)
            if (b->cell[q] == EMPTY && b->pat4[color][q] == p4) return q;
    }
    return -1;
}

// color が (禁じ手でなく) 打てる達四・四四の点があるか。あればその点、なければ -1
static int legal_flex4(Board *b, int color) {
    if (!b->p4cnt[color][P4_FLEX4]) return -1;
    for (int y = 0; y < b->size; y++) {
        int q = to_pos(0, y);
        for (int x = 0; x < b->size; x++, q++)
            if (b->cell[q] == EMPTY && b->pat4[color][q] == P4_FLEX4 && legal(b, color, q)) return q;
    }
    return -1;
}

int evaluate(const Board *b) {
    int me = board_side(b), op = opp_color(me);
    const EvalParams *w = &g_eval_params;
    long v = b->vsum[me] - (long)b->vsum[op] * w->opp_scale / 64 + w->tempo;
    const int (*th)[THREAT_COUNT] = w->threat[me == BLACK ? 0 : 1];
    int bm = eval_threat_bits(b->p4cnt[me]), bo = eval_threat_bits(b->p4cnt[op]);
    for (int k = 0; k < THREAT_COUNT; k++) {
        if (bm >> k & 1) v += th[0][k];
        if (bo >> k & 1) v += th[1][k];
    }
    if (w->cnt_f3_me | w->cnt_f3_op | w->cnt_b4_me | w->cnt_b4_op) {
        const int16_t *cm = b->p4cnt[me], *co = b->p4cnt[op];
        int f3m = cm[P4_FLEX3] + cm[P4_FLEX3_PLUS] + cm[P4_FLEX3_2X], f3o = co[P4_FLEX3] + co[P4_FLEX3_PLUS] + co[P4_FLEX3_2X];
        int b4m = cm[P4_BLOCK4] + cm[P4_BLOCK4_PLUS] + cm[P4_BLOCK4_FLEX3], b4o = co[P4_BLOCK4] + co[P4_BLOCK4_PLUS] + co[P4_BLOCK4_FLEX3];
        if (f3m > 8) f3m = 8;
        if (f3o > 8) f3o = 8;
        if (b4m > 8) b4m = 8;
        if (b4o > 8) b4o = 8;
        v += f3m * w->cnt_f3_me - f3o * w->cnt_f3_op + b4m * w->cnt_b4_me - b4o * w->cnt_b4_op;
    }
    if (w->race) {   // 脅威の速さの比べ (eval.h、既定は無効)
        int m = eval_threat_level(b->p4cnt[me]), o = eval_threat_level(b->p4cnt[op]);
        v += o >= m ? w->race_own[m] : -w->race_opp[o];
    }
    if (v > 20000) v = 20000;
    if (v < -20000) v = -20000;
    return (int)v;
}

// 調整できる探索の値 (INFO p_<名前> <値> で変えられる。search_set_param)
static struct {
    int qdepth;     // 静止探索の深さの限度 (四と防ぎの手数)
    int q_min_p4;   // 静止探索で読む自分の四の最低の形 (P4_BLOCK4 / P4_BLOCK4_PLUS / P4_BLOCK4_FLEX3)
    int soft_pct;   // 上限時間の何 % を過ぎたら次の反復を始めないか (think.c が使う)
    int vct_pct;    // ルートの VCT に使う時間 (上限時間の %)。0 = 使わない
    int vct_depth;  // ルートの VCT の深さ (攻めの手数)
    int def_restrict;   // 1 = 相手に VCF があればルートの候補を vcf_defenses の手 (受けの上位集合) に絞る
    int lmp_mul;        // 浅い所の手数打ち切りの手数の倍率 (%)
    int lmr_moves;      // 何手目から深さを削るか
    int cand_min;       // ルート以外で、点 (攻め+守り) がこれ未満の戦術的でない手は 2 手目以降読まない
    int ext_three;      // 1 = 活三 (相手に受けを強いる手) は深さを減らさない (延長)。延長はルートの深さの 2 倍まで。2 = 半手だけ延長 (2 手で 1 手分)
    int vctdef_pct;     // 相手の VCT への受け (vct_defenses) に使う時間 (上限時間の %)。0 = 使わない
    int unstable_pct;   // 直前の反復で最善手が変わったら、次の反復を始めない目安を何 % に延ばすか
    int leaf_vcf;       // 末端 (深さ 0) で手番側に四があれば VCF を調べるノード数の上限。0 = 調べない
    int leaf_vcf_depth; // 末端の VCF の深さ (攻めの手数)
    int tz;             // 1 = 脅威圏探索: 深さ tz_depth 以上の節点で、パスしたら相手に VCF があれば手を圏 (tz.c) に絞る
    int tz_depth;
    int tz_nodes;       // 脅威圏の VCF 探索のノード数の上限
    int tz_ext;         // 1 = 圏に絞った節点では深さを減らさない (延長。ルートの深さの 2 倍まで)
    int nmp;            // 1 = 脅威を確かめた null move 枝刈り: パスしても相手に VCF がなく、パスして浅く読んでも beta 以上なら打ち切る
    int nmp_depth;      // null move を試す最低の深さ
    int nmp_r;          // null move の深さの削減 (基本値。深さ 6 ごとに +1)
    int own_vcf;        // 深さ own_vcf 以上の節点で手番側の VCF を調べ、あれば勝ち (0 = 調べない)
    int ext_four;       // 1 = 四を作る手 (相手は止めるしかない) も深さを減らさない (延長。ルートの深さの 2 倍まで)
    int ext_limit;      // 延長の上限: ply + depth < ext_limit * ルートの深さ / 2
    int lmr_log;        // 0 = 従来の LMR。正なら削減 = ln(深さ) * ln(手の順番) * 100 / lmr_log (深い所ほど遅い手を大きく削る)
    int lmr_pv;         // lmr_log のとき PV の節点での削減を何 % にするか
    int tact_own;       // 自分のこの形 (Pattern4) 以上を作る手を「戦術的」(LMP・LMR をしない) とする
    int tact_def;       // 相手のこの形以上の点をふさぐ手を「戦術的」とする
    int def_own4;       // 0 = 相手の活三を受ける節点で、自分に VCF がなければ受けにならない自分のただの四を読まない
    int def_vcf_nodes, def_vcf_depth, def_vcf_min, def_own4_depth;   // その VCF の確かめのノード数・深さ
    int lmr_pol, lmr_pol_max;   // policy のとき、点が最大の手より lmr_pol 低いごとに LMR を 1 手増やす (最大 lmr_pol_max)。0 = しない
    int policy;         // 1 = 手の並べ方に policy_data.c の重みを使う、2 = 四を作る手は今の点のまま
    int def_line;       // 1 = 相手の活三への受けを、達四の点を通る線の上の点 (と自分の四) に絞る
    int vct_dfpn;       // 1 = ルートの VCT (vct_pct) を証明数探索 (dfpn.c) で調べる (既定。0 = vct_solve)
    int smp_pick;       // 1 = 補助スレッドの結果 (勝ちの読み切り・より深い反復) も使う
    int dfdef_pct;      // 相手の VCT への受けを df-pn で調べる時間 (上限時間の %)。0 = 使わない
    int dfver_pct;      // 探索で選んだ手を打った後に相手の VCT がないか df-pn で確かめる時間 (上限時間の %)。0 = 使わない
    int rz, rz_depth, rz_after, rz_len, rz_rad, rz_lines, rz_red;   // 反証の圏 (rz.h)。rz: 0 = 使わない、1 = 圏の外の静かな手を rz_red 縮める、2 = 読まない
    int def_scale;  // 手の並べ方の守り (相手の点) の重み /64
    int lmr_tact;   // >0: 戦術的な手 (四・三三・相手の活三への受けを除く) も searched >= lmr_tact なら 1 手浅く読む (切れなければ読み直す)
    int ttfirst;    // 1 = 置換表の手を手の生成の前に読み、切れなければ生成する (段階的な手の生成。速さだけ)
    int tfut, tfut_margin;   // 残り深さ 1 の節点で評価が alpha - tfut_margin 以下なら、四でない手 (1 = 自分のただの三も、2 = 相手の三の点の受けも) を読まない
    int vz, vz_depth, vz_nodes, vz_keep, vz_pv, vz_root, vz_root_nodes, vz_tact;   // 相手の VCT の証明の圏 (dfpn_vct_zone) の外の手を読まない。vz_keep 以上の自分の形を作る手 (反撃) は残す
    int mcp, mcp_mul, mcp_tact, mcp_pv;   // 全部の深さの手の数の枝刈り (Rapfi 風): 3 + 深さ^1.3 手 (評価が良くなっていなければ半分) を超えたら読まない。倍率 %、戦術的な手・PV にも掛けるか
    int fut, fut_margin, fut_depth;      // futility: 評価 − margin × (深さ − 1 − 改善) ≥ beta なら評価を返す (PV・相手の活三があるときは除く)
    int razor, razor_margin, razor_depth; // razoring: 評価 + margin × 深さ < alpha なら静止探索の値を返す
    int ts_tree_root;   // >0: 根の手番側の前の反復の点がこれ以下に負けているときだけ本探索の中の ts を使う
    int ts_tree_pv;   // 1 = PV の局面だけ
    int ts_tree_fail;   // 1 = 勝てなかった局面を覚えて呼び直さない
    int ts_tree_side;   // 1 = 根の相手の勝ちだけ調べる (奇数手目の局面)
    int ts_tree_tt;   // 本探索の中の ts で証明した勝ちを置換表に残すか
    int ts_tree, ts_tree_depth, ts_tree_plies, ts_tree_eval;   // 本探索の中の ts: ノード上限 (0 = 使わない)、残りの深さの下限、攻めの手数、静的評価の下限
    int tpo, tpo_depth, tpo_own, tpo_opp;   // 脅威の潜在力で手を並べる (1 = 加点、2 = 加点して戦術手扱い)、残りの深さの下限、加点
    int tpv_lazy;   // tpv == 2 で静的な値による見切りをするか
    int tpv;   // 1 = 静止探索の葉の評価に VCF の有無の補正 (tp.c の tp_vcf_term) を足す、2 = 残り深さ 1 の局面で 1 回だけ足す
    int tp, tp_min_depth;   // 脅威の潜在力 (tp.c) を根から tp 手目の局面の値に足す (0 = 使わない)、その局面の残りの深さの下限
    int ts_pct;         // ルートで自分の勝ちを αβ の脅威探索 (ts.c) で調べる時間 (上限時間の %)。0 = 使わない
    int dfver_ts;       // 選んだ手の確かめ (dfver_pct): 1 = df-pn と ts に半分ずつ、2 = ts だけ
    int dfver_l3;       // 1 = その確かめを λ3 (l3.c: 静かな脅威を含む勝ち) で行う (重い。長い持ち時間向け)
} sp = {
    .ts_tree = 0, .ts_tree_tt = 1, .ts_tree_side = 0, .ts_tree_fail = 1, .ts_tree_pv = 0, .ts_tree_root = 0, .mcp = 0, .mcp_mul = 100, .mcp_tact = 0, .mcp_pv = 0, .fut = 0, .fut_margin = 250, .fut_depth = 99, .razor = 0, .razor_margin = 100, .razor_depth = 2, .ts_tree_depth = 3, .ts_tree_plies = 6, .ts_tree_eval = 300, .ts_pct = 20, .tpv = 0, .tpv_lazy = 1, .tp = 0, .tp_min_depth = 1, .tpo = 0, .tpo_depth = 3, .tpo_own = 1 << 20, .tpo_opp = 1 << 19, .dfver_ts = 0, .rz = 0, .rz_depth = 3, .rz_after = 2, .rz_len = 6, .rz_rad = 1, .rz_lines = 1, .rz_red = 2,
    .def_scale = 52, .lmr_tact = 0, .ttfirst = 1, .tfut = 0, .tfut_margin = 600,
    .vz = 0, .vz_depth = 4, .vz_nodes = 2000, .vz_keep = P4_BLOCK3, .vz_pv = 1, .vz_root = 0, .vz_root_nodes = 20000, .vz_tact = 0,
    .smp_pick = 1, .dfdef_pct = 0, .dfver_pct = 0,
    .qdepth = 0, .q_min_p4 = P4_BLOCK4_PLUS, .soft_pct = 60, .vct_pct = 20,
    .vct_depth = 10, .def_restrict = 1, .lmp_mul = 100, .lmr_moves = 3,
    .cand_min = 0, .ext_three = 1, .vctdef_pct = 0, .unstable_pct = 100,
    .leaf_vcf = 0, .leaf_vcf_depth = 12, .tz = 0, .tz_depth = 2,
    .tz_nodes = 300, .tz_ext = 0, .nmp = 0, .nmp_depth = 3,
    .nmp_r = 2, .own_vcf = 0, .ext_four = 0, .ext_limit = 4,
    .lmr_log = 0, .lmr_pv = 66, .tact_own = P4_FLEX3, .tact_def = P4_FLEX3,
    .vct_dfpn = 1, .def_line = 0, .policy = 0, .lmr_pol = 0, .lmr_pol_max = 2, .def_own4 = 0, .def_vcf_nodes = 300, .def_vcf_depth = 16, .def_vcf_min = 99, .def_own4_depth = 2,
};

int search_soft_pct(void) { return sp.soft_pct; }

// 対数の LMR の表 [深さ][何手目]
static uint8_t lmr_table[64][64];
// 自然対数 (libm を使わない): x = 2^k * y (1 <= y < 2)、ln y = 2 atanh((y-1)/(y+1))
static double ln_(double x) {
    int k = 0;
    while (x >= 2) { x /= 2; k++; }
    double t = (x - 1) / (x + 1), t2 = t * t, s = 0, p = t;
    for (int i = 1; i < 30; i += 2) { s += p / i; p *= t2; }
    return k * 0.6931471805599453 + 2 * s;
}
// 手の数の枝刈りの表 [深さ] = (3 + 深さ^1.3) × mcp_mul / 100 (深さ^1.3 = exp(1.3 ln d))
static int mcp_table[64];
static double exp_(double x) {
    int k = 0;
    while (x > 0.5) { x /= 2; k++; }
    double s = 1, t = 1;
    for (int i = 1; i < 20; i++) { t *= x / i; s += t; }
    while (k-- > 0) s *= s;
    return s;
}
static void mcp_init(void) {
    for (int d = 0; d < 64; d++) mcp_table[d] = (int)((3 + (d ? exp_(1.3 * ln_(d)) : 0)) * sp.mcp_mul / 100);
}
static int lmr_table_ok;
static void lmr_init(void) {
    for (int d = 0; d < 64; d++)
        for (int m = 0; m < 64; m++) {
            double r = (d && m && sp.lmr_log > 0) ? ln_(d) * ln_(m) * 100.0 / sp.lmr_log : 0;
            lmr_table[d][m] = (uint8_t)(r + 0.5 > 30 ? 30 : r + 0.5);
        }
    lmr_table_ok = 1;
}

int search_set_param(const char *name, long v) {
    if (!strcmp(name, "qdepth")) sp.qdepth = (int)v;
    else if (!strcmp(name, "q_min_p4")) sp.q_min_p4 = (int)v;
    else if (!strcmp(name, "soft_pct")) sp.soft_pct = (int)v;
    else if (!strcmp(name, "vct_pct")) sp.vct_pct = (int)v;
    else if (!strcmp(name, "vct_depth")) sp.vct_depth = (int)v;
    else if (!strcmp(name, "def_restrict")) sp.def_restrict = (int)v;
    else if (!strcmp(name, "lmp_mul")) sp.lmp_mul = (int)v;
    else if (!strcmp(name, "lmr_moves")) sp.lmr_moves = (int)v;
    else if (!strcmp(name, "cand_min")) sp.cand_min = (int)v;
    else if (!strcmp(name, "ext_three")) sp.ext_three = (int)v;
    else if (!strcmp(name, "vctdef_pct")) sp.vctdef_pct = (int)v;
    else if (!strcmp(name, "unstable_pct")) sp.unstable_pct = (int)v;
    else if (!strcmp(name, "threads")) search_set_threads((int)v);
    else if (!strcmp(name, "smp_pick")) sp.smp_pick = (int)v;
    else if (!strcmp(name, "dfdef_pct")) sp.dfdef_pct = (int)v;
    else if (!strcmp(name, "dfver_pct")) sp.dfver_pct = (int)v;
    else if (!strcmp(name, "dfver_l3")) sp.dfver_l3 = (int)v;
    else if (!strcmp(name, "rz")) sp.rz = (int)v;
    else if (!strcmp(name, "ts_pct")) sp.ts_pct = (int)v;
    else if (!strcmp(name, "tp")) sp.tp = (int)v;
    else if (!strcmp(name, "tpv")) sp.tpv = (int)v;
    else if (!strcmp(name, "tpv_lazy")) sp.tpv_lazy = (int)v;
    else if (!strcmp(name, "tpv_own")) g_tpv_own = (int)v;
    else if (!strcmp(name, "tpv_opp")) g_tpv_opp = (int)v;
    else if (!strcmp(name, "tpv_nodes")) g_tpv_nodes = v;
    else if (!strcmp(name, "tpo")) sp.tpo = (int)v;
    else if (!strcmp(name, "tpo_depth")) sp.tpo_depth = (int)v;
    else if (!strcmp(name, "tpo_own")) sp.tpo_own = (int)v;
    else if (!strcmp(name, "tpo_opp")) sp.tpo_opp = (int)v;
    else if (!strcmp(name, "tp_min_depth")) sp.tp_min_depth = (int)v;
    else if (!strcmp(name, "tp_own")) g_tp_own = (int)v;
    else if (!strcmp(name, "tp_opp")) g_tp_opp = (int)v;
    else if (!strcmp(name, "tp_lo")) g_tp_lo = (int)v;
    else if (!strcmp(name, "tp_cap")) g_tp_cap = (int)v;
    else if (!strcmp(name, "tp_nodes")) g_tp_nodes = v;
    else if (!strcmp(name, "ts_tree")) sp.ts_tree = (int)v;
    else if (!strcmp(name, "ts_tree_tt")) sp.ts_tree_tt = (int)v;
    else if (!strcmp(name, "ts_tree_side")) sp.ts_tree_side = (int)v;
    else if (!strcmp(name, "ts_tree_fail")) sp.ts_tree_fail = (int)v;
    else if (!strcmp(name, "ts_tree_pv")) sp.ts_tree_pv = (int)v;
    else if (!strcmp(name, "ts_tree_root")) sp.ts_tree_root = (int)v;
    else if (!strcmp(name, "mcp")) sp.mcp = (int)v, mcp_init();
    else if (!strcmp(name, "mcp_mul")) sp.mcp_mul = (int)v, mcp_init();
    else if (!strcmp(name, "mcp_tact")) sp.mcp_tact = (int)v;
    else if (!strcmp(name, "mcp_pv")) sp.mcp_pv = (int)v;
    else if (!strcmp(name, "fut")) sp.fut = (int)v;
    else if (!strcmp(name, "fut_margin")) sp.fut_margin = (int)v;
    else if (!strcmp(name, "fut_depth")) sp.fut_depth = (int)v;
    else if (!strcmp(name, "razor")) sp.razor = (int)v;
    else if (!strcmp(name, "razor_margin")) sp.razor_margin = (int)v;
    else if (!strcmp(name, "razor_depth")) sp.razor_depth = (int)v;
    else if (!strcmp(name, "ts_tree_depth")) sp.ts_tree_depth = (int)v;
    else if (!strcmp(name, "ts_tree_plies")) sp.ts_tree_plies = (int)v;
    else if (!strcmp(name, "ts_tree_eval")) sp.ts_tree_eval = (int)v;
    else if (!strcmp(name, "dfver_ts")) sp.dfver_ts = (int)v;
    else if (!strcmp(name, "rz_depth")) sp.rz_depth = (int)v;
    else if (!strcmp(name, "rz_after")) sp.rz_after = (int)v;
    else if (!strcmp(name, "rz_len")) sp.rz_len = (int)v;
    else if (!strcmp(name, "rz_rad")) sp.rz_rad = (int)v;
    else if (!strcmp(name, "rz_lines")) sp.rz_lines = (int)v;
    else if (!strcmp(name, "rz_red")) sp.rz_red = (int)v;
    else if (!strcmp(name, "leaf_vcf")) sp.leaf_vcf = (int)v;
    else if (!strcmp(name, "leaf_vcf_depth")) sp.leaf_vcf_depth = (int)v;
    else if (!strcmp(name, "tz")) sp.tz = (int)v;
    else if (!strcmp(name, "vz")) sp.vz = (int)v;
    else if (!strcmp(name, "tfut")) sp.tfut = (int)v;
    else if (!strcmp(name, "ttfirst")) sp.ttfirst = (int)v;
    else if (!strcmp(name, "lmr_tact")) sp.lmr_tact = (int)v;
    else if (!strcmp(name, "def_scale")) sp.def_scale = (int)v;
    else if (!strcmp(name, "tfut_margin")) sp.tfut_margin = (int)v;
    else if (!strcmp(name, "vz_depth")) sp.vz_depth = (int)v;
    else if (!strcmp(name, "vz_nodes")) sp.vz_nodes = (int)v;
    else if (!strcmp(name, "vz_keep")) sp.vz_keep = (int)v;
    else if (!strcmp(name, "vz_pv")) sp.vz_pv = (int)v;
    else if (!strcmp(name, "vz_tact")) sp.vz_tact = (int)v;   // 1 = 圏の中の手を戦術的な手として枝刈り・LMR から守る
    else if (!strcmp(name, "vz_root")) sp.vz_root = (int)v;
    else if (!strcmp(name, "vz_root_nodes")) sp.vz_root_nodes = (int)v;
    else if (!strcmp(name, "tz_depth")) sp.tz_depth = (int)v;
    else if (!strcmp(name, "tz_nodes")) sp.tz_nodes = (int)v;
    else if (!strcmp(name, "tz_ext")) sp.tz_ext = (int)v;
    else if (!strcmp(name, "nmp")) sp.nmp = (int)v;
    else if (!strcmp(name, "nmp_depth")) sp.nmp_depth = (int)v;
    else if (!strcmp(name, "nmp_r")) sp.nmp_r = (int)v;
    else if (!strcmp(name, "own_vcf")) sp.own_vcf = (int)v;
    else if (!strcmp(name, "ext_four")) sp.ext_four = (int)v;
    else if (!strcmp(name, "ext_limit")) sp.ext_limit = (int)v;
    else if (!strcmp(name, "lmr_log")) { sp.lmr_log = (int)v; lmr_table_ok = 0; }
    else if (!strcmp(name, "lmr_pv")) sp.lmr_pv = (int)v;
    else if (!strcmp(name, "tact_own")) sp.tact_own = (int)v;
    else if (!strcmp(name, "tact_def")) sp.tact_def = (int)v;
    else if (!strcmp(name, "vct_dfpn")) sp.vct_dfpn = (int)v;
    else if (!strcmp(name, "def_line")) sp.def_line = (int)v;
    else if (!strcmp(name, "policy")) sp.policy = (int)v;
    else if (!strcmp(name, "lmr_pol")) sp.lmr_pol = (int)v;
    else if (!strcmp(name, "lmr_pol_max")) sp.lmr_pol_max = (int)v;
    else if (!strcmp(name, "def_own4")) sp.def_own4 = (int)v;
    else if (!strcmp(name, "def_vcf_nodes")) sp.def_vcf_nodes = (int)v;
    else if (!strcmp(name, "def_vcf_depth")) sp.def_vcf_depth = (int)v;
    else if (!strcmp(name, "def_vcf_min")) sp.def_vcf_min = (int)v;
    else if (!strcmp(name, "def_own4_depth")) sp.def_own4_depth = (int)v;
    else if (!strcmp(name, "dfpn_vcf")) g_dfpn_vcf_nodes = v;
    else if (!strcmp(name, "fvcf")) g_fvcf = (int)v;
    else if (!strcmp(name, "ts_light")) g_ts_light = (int)v;
    else if (!strcmp(name, "opp_scale")) g_eval_params.opp_scale = (int)v;
    else if (!strcmp(name, "tempo")) g_eval_params.tempo = (int)v;
    else if (!strcmp(name, "table")) g_eval_params.use_table = (int)v;
    else if (!strcmp(name, "race")) g_eval_params.race = (int)v;
    else if (!strcmp(name, "cnt_f3_me")) g_eval_params.cnt_f3_me = (int)v;
    else if (!strcmp(name, "cnt_f3_op")) g_eval_params.cnt_f3_op = (int)v;
    else if (!strcmp(name, "cnt_b4_me")) g_eval_params.cnt_b4_me = (int)v;
    else if (!strcmp(name, "cnt_b4_op")) g_eval_params.cnt_b4_op = (int)v;
    else if (!strncmp(name, "race_own", 8) && name[8] >= '0' && name[8] < '0' + EVAL_RACE_LEVELS) g_eval_params.race_own[name[8] - '0'] = (int)v;
    else if (!strncmp(name, "race_opp", 8) && name[8] >= '0' && name[8] < '0' + EVAL_RACE_LEVELS) g_eval_params.race_opp[name[8] - '0'] = (int)v;
    else if (!strncmp(name, "threat", 6) && strlen(name) == 9) {
        // threat<手番 0=黒 1=白><0=手番側 1=相手側><種類>  例: threat010
        int a = name[6] - '0', s2 = name[7] - '0', k = name[8] - '0';
        if (a < 0 || a > 1 || s2 < 0 || s2 > 1 || k < 0 || k >= THREAT_COUNT) return 0;
        g_eval_params.threat[a][s2][k] = (int)v;
    }
    else if (!strncmp(name, "line", 4) && atoi(name + 4) >= 0 && atoi(name + 4) < P_COUNT) g_eval_params.line[atoi(name + 4)] = (int)v;
    else if (!strncmp(name, "combo", 5) && atoi(name + 5) >= 0 && atoi(name + 5) < P4_COUNT) g_eval_params.combo[atoi(name + 5)] = (int)v;
    else return 0;
    return 1;
}

// ---------------------------------------------------------------- 手生成

typedef struct {
    int pos;
    int score;
    int value;      // パターン点 (攻め + 守り)。history などを含まない
    int tactical;   // 三・四を作る、または相手の三・四の点をふさぐ
} Move;

#define DEF_SCALE sp.def_scale    // 守り (相手の点) の重み /64 (既定 52、INFO p_def_scale)

// 手の並べ方の重み (policy_data.c、Rapfi の最善手に合わせた線パターン・Pattern4 の点) を pattern_index で引ける形に広げたもの。
// [手番の色][0 = 自分のパターン、1 = 相手のパターン][pattern_index]。g_p4_table はルールで変わるので search_root で作り直す
static int16_t pol_tab[3][2][PIDX_COUNT];
static int pol_ok, pol_rule;
static void policy_init(void) {
    for (int i = 0; i < PIDX_COUNT; i++) {
        int p[4] = {i / (P_COUNT * P_COUNT * P_COUNT), i / (P_COUNT * P_COUNT) % P_COUNT, i / P_COUNT % P_COUNT, i % P_COUNT};
        for (int c = 1; c <= 2; c++) {
            int me = g_policy_p4_me[c - 1][g_p4_table[i]], op = g_policy_p4_op[c - 1][g_p4_table[i]];
            for (int d = 0; d < 4; d++) {
                me += g_policy_line_me[c - 1][p[d]];
                op += g_policy_line_op[c - 1][p[d]];
            }
            pol_tab[c][0][i] = (int16_t)me;
            pol_tab[c][1][i] = (int16_t)op;
        }
    }
    pol_ok = 1;
}

static inline int cheb_dist(int a, int b) {
    int dx = pos_x(a) - pos_x(b), dy = pos_y(a) - pos_y(b);
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    int d = dx > dy ? dx : dy;
    return d > 6 ? 6 : d;
}

// 相手の四・活三への防ぎ手か: 相手がそこに打つとその線で四以上になる点
static inline int blocks_threat(const Board *b, int op, int q) {
    const uint8_t *pt = b->pat[op][q];
    for (int d = 0; d < 4; d++)
        if (pt[d] >= P_B4 && pt[d] <= P_D4) return 1;
    return 0;
}

// mode: 0 = 通常 (近くの空点すべて。op_threat なら防ぎ手と自分の四だけ)、1 = 静止探索 (自分の四だけ)
// tzon: 脅威圏 (c->tzmark == c->tzgen の点) と自分の四だけ
static int gen_moves(Ctx *c, Move *out, int me, int op, int op_threat, int ttmove, int ply, int mode, int tzon) {
    Board *b = c->b;
    int n = 0;
    if (b->nmoves == 0) {
        out[0].pos = to_pos(b->size / 2, b->size / 2);
        out[0].score = 0;
        out[0].tactical = 0;
        out[0].value = 0;
        return 1;
    }
    const int *k = ply < MAX_PLY ? c->killer[ply] : NULL;
    // 石のある範囲 ±2 だけを見る
    int x0 = b->size, x1 = -1, y0 = b->size, y1 = -1;
    for (int i = 0; i < b->nmoves; i++) {
        int x = pos_x(b->moves[i]), y = pos_y(b->moves[i]);
        if (x < x0) x0 = x;
        if (x > x1) x1 = x;
        if (y < y0) y0 = y;
        if (y > y1) y1 = y;
    }
    x0 = x0 > 2 ? x0 - 2 : 0, y0 = y0 > 2 ? y0 - 2 : 0;
    x1 = x1 + 2 < b->size ? x1 + 2 : b->size - 1, y1 = y1 + 2 < b->size ? y1 + 2 : b->size - 1;
    const int16_t *vme = g_cell_value[me], *vop = g_cell_value[op];
    const int *hist = c->history[me];
    int polon = sp.policy && b->nmoves >= 2;
    int lastm = polon ? b->moves[b->nmoves - 1] : 0, mylast = polon ? b->moves[b->nmoves - 2] : 0;
    int defl = op_threat && sp.def_line;
    if (defl) {
        // 相手の活三 (達四・四四の点) への受けは、その点を通る四の線の上で相手が四を作れる点だけ (他の点では線は変わらない)
        c->tzgen++;
        for (int y = y0; y <= y1; y++) {
            int q = to_pos(x0, y);
            for (int x = x0; x <= x1; x++, q++) {
                if (b->cell[q] != EMPTY || b->pat4[op][q] != P4_FLEX4 || !legal(b, op, q)) continue;
                c->tzmark[q] = c->tzgen;
                for (int d = 0; d < 4; d++) {
                    if (b->pat[op][q][d] < P_B4 || b->pat[op][q][d] > P_D4) continue;
                    for (int k = -4; k <= 4; k++) {
                        int r = q + k * DIR_OFFSET[d];
                        if (r < 0 || r >= NCELL || b->cell[r] != EMPTY) continue;
                        if (b->pat[op][r][d] >= P_B4 && b->pat[op][r][d] <= P_D4) c->tzmark[r] = c->tzgen;
                    }
                }
            }
        }
    }
    for (int y = y0; y <= y1; y++) {
        int q = to_pos(x0, y);
        for (int x = x0; x <= x1; x++, q++) {
            if (b->cell[q] != EMPTY || !b->near[q]) continue;
            int a = b->pat4[me][q], d = b->pat4[op][q];
            if (mode == 1 && (a < sp.q_min_p4 || a > P4_BLOCK4_FLEX3)) continue;
            if (op_threat && a < P4_BLOCK4 && !blocks_threat(b, op, q)) continue;
            if (defl && a < P4_BLOCK4 && c->tzmark[q] != c->tzgen) continue;
            if (c->drop4 && (a == P4_BLOCK4 || a == P4_BLOCK4_PLUS) && !blocks_threat(b, op, q)) continue;
            if (tzon && a < P4_BLOCK4 && c->tzmark[q] != c->tzgen) continue;
            int s = vme[board_pidx(b, me, q)] + vop[board_pidx(b, op, q)] * DEF_SCALE / 64;
            if (polon && (sp.policy == 1 || a < P4_BLOCK4)) {
                // 表の点 (1 logit = 256) を今の点の尺度 (100 ≒ 1 logit) に直す
                int pv = s * g_policy_base_scale / 100 + pol_tab[me][0][board_pidx(b, me, q)] +
                         pol_tab[me][1][board_pidx(b, op, q)] + g_policy_dist_last[cheb_dist(q, lastm)] +
                         g_policy_dist_mine[cheb_dist(q, mylast)];
                s = pv * 100 / 256;
            }
            out[n].value = s;
            s = s * 16 + hist[q] / 64;
            if (q == ttmove) s += 1 << 28;
            else if (k && (q == k[0] || q == k[1])) s += 1 << 12;
            out[n].pos = q;
            out[n].score = s;
            out[n].tactical = a >= sp.tact_own || d >= sp.tact_def;
            n++;
        }
    }
    return n;
}

// 挿入ソート (降順)。ルート用
static void sort_moves(Move *m, int n) {
    for (int i = 1; i < n; i++) {
        Move t = m[i];
        int j = i - 1;
        while (j >= 0 && m[j].score < t.score) { m[j + 1] = m[j]; j--; }
        m[j + 1] = t;
    }
}

// 置換表の手が gen_moves の出す手か (ttfirst 用。gen_moves の絞り込みと同じ条件)
static int gen_ok(Ctx *c, int q, int me, int op, int op_threat, int mode) {
    Board *b = c->b;
    if (q < 0 || q >= NCELL || b->nmoves == 0 || b->cell[q] != EMPTY || !b->near[q]) return 0;
    int a = b->pat4[me][q];
    if (mode == 1 && (a < sp.q_min_p4 || a > P4_BLOCK4_FLEX3)) return 0;
    if (op_threat && a < P4_BLOCK4 && !blocks_threat(b, op, q)) return 0;
    if (c->drop4 && (a == P4_BLOCK4 || a == P4_BLOCK4_PLUS) && !blocks_threat(b, op, q)) return 0;
    return 1;
}

// i 番目以降で最も点の高い手を i 番目に持ってくる (枝刈りで早く打ち切ることが多いので全部は並べない)
static inline void pick_move(Move *m, int i, int n) {
    int bi = i;
    for (int j = i + 1; j < n; j++)
        if (m[j].score > m[bi].score) bi = j;
    if (bi != i) { Move t = m[i]; m[i] = m[bi]; m[bi] = t; }
}

// ---------------------------------------------------------------- PVS

static inline void check_time(Ctx *c) {
    if ((c->nodes & 1023) == 0) {
        if (c->id == 0 && now_ms() - c->t0 >= c->hard_ms) c->stop = 1;
        if (__atomic_load_n(&g_stop, __ATOMIC_RELAXED)) c->stop = 1;
    }
    if (c->max_nodes && c->nodes >= c->max_nodes) c->stop = 1;
}

// 浅い深さで、戦術的でない手を何手まで調べるか
static const int LMP_COUNT[5] = {0, 6, 10, 16, 24};


#define TSFAIL_SIZE (1u << 15)
static _Thread_local uint64_t tsfail[TSFAIL_SIZE];   // 本探索の中の ts で勝てなかった局面 (予算込みのキー)
static _Thread_local int tstree_gate = 1;   // 本探索の中の ts を使う反復か (ts_tree_root)
long tstree_stat[12];   // 本探索の中の ts: 呼んだ回数、ノード、残りの深さ別 [2+d]、勝ち [10]
long nmp_stat[4];
long vz_stat[6];   // 相手の VCT の圏: 調べた回数、勝ち、圏が取れた、絞る前の手の数、絞った後の手の数、ルートで外した手
// 圏の外でも読む手: 自分の四の点を増やす形 (止め三以上。三三の点だけの活二×2 は除く) — 証明木の受け側の四が変わり得る
static inline int vz_keep_move(const Board *b, int me, int q) {
    int a = b->pat4[me][q];
    return a >= sp.vz_keep && !(sp.vz_keep <= P4_FLEX2_2X && a == P4_FLEX2_2X);
}
// 相手に VCT の元 (三・四を作れる点) があるか
static inline int vz_gate(const Board *b, int op) {
    const int16_t *c = b->p4cnt[op];
    for (int k = P4_FLEX3; k <= P4_BLOCK4_FLEX3; k++)
        if (c[k]) return 1;
    return 0;
}
// 相手 (op) にパスしたら VCT があるとき、その証明の圏に印を付ける (c->tzmark == c->tzgen)。圏が取れたら 1
static int vz_mark(Ctx *c, Board *b, int op, long nodes) {
    DfpnResult r;
    vz_stat[0]++;
    if (!dfpn_vct(b, op, nodes, 0, &r)) return 0;
    vz_stat[1]++;
    static _Thread_local int cells[NCELL];
    int nz = dfpn_vct_zone(b, op, cells, NCELL);
    if (nz < 0) return 0;
    vz_stat[2]++;
    c->tzgen++;
    for (int i = 0; i < nz; i++) c->tzmark[cells[i]] = c->tzgen;
    return 1;
}   // 調べた回数 (g2-test speed が表示)
static _Thread_local int tp_inside;   // 脅威の潜在力の補正を足している局面の中か

static _Thread_local int tpv_inside;  // 残り深さ 1 の局面の旗の補正を足している局面の中か

static int pvs(Ctx *c, int alpha, int beta, int depth, int ply, int is_pv) {
    Board *b = c->b;
    // tpv == 2: 葉ではなく残り深さ 1 の局面で 1 回だけ VCF の旗 (tp.c) を調べ、補正の分だけ窓をずらして読み、値に足す
    if (sp.tpv == 2 && depth == 1 && !tpv_inside && alpha > -SCORE_WIN_MIN && beta < SCORE_WIN_MIN &&
        !b->p4cnt[BLACK][P4_FIVE] && !b->p4cnt[WHITE][P4_FIVE]) {
        // 静的な値に補正を足しても窓の外のままなら調べない (葉の版と同じ見切り、tpv_lazy = 0 で常に調べる)
        int stat = sp.tpv_lazy ? evaluate(b) : 0;
        int corr = sp.tpv_lazy && (stat - g_tpv_opp >= beta || stat + g_tpv_own <= alpha) ? 0 : tp_vcf_term(b);
        if (corr) {
            tpv_inside = 1;
            int v = pvs(c, alpha - corr, beta - corr, depth, ply, is_pv);
            tpv_inside = 0;
            return v > SCORE_WIN_MIN || v < -SCORE_WIN_MIN ? v : v + corr;
        }
    }
    // 脅威の潜在力 (tp.c): 根から sp.tp 手目の局面では、補正の分だけ窓をずらして読み、値に補正を足す
    if (sp.tp && ply == sp.tp && depth >= sp.tp_min_depth && !tp_inside && alpha > -SCORE_WIN_MIN && beta < SCORE_WIN_MIN &&
        !b->p4cnt[BLACK][P4_FIVE] && !b->p4cnt[WHITE][P4_FIVE]) {
        int corr = tp_correction(b);
        tp_inside = 1;
        int v = pvs(c, alpha - corr, beta - corr, depth, ply, is_pv);
        tp_inside = 0;
        return v > SCORE_WIN_MIN || v < -SCORE_WIN_MIN ? v : v + corr;
    }
    int me = board_side(b), op = opp_color(me);
    c->pvlen[ply] = 0;
    c->nodes++;
    check_time(c);
    if (c->stop) return 0;
    if (ply > c->seldepth) c->seldepth = ply;

    // 自分の五
    if (b->p4cnt[me][P4_FIVE]) return SCORE_WIN - ply;
    if (b->nmoves >= b->size * b->size) return 0;
    if (ply >= MAX_PLY - 2) return evaluate(b);

    // 詰みの距離による枝刈り
    if (alpha < -(SCORE_WIN - ply)) alpha = -(SCORE_WIN - ply);
    if (beta > SCORE_WIN - ply) beta = SCORE_WIN - ply;
    if (alpha >= beta) return alpha;

    // 相手の五の点: 止めるしかない (深さは減らさない)
    int opfive = b->p4cnt[op][P4_FIVE];
    if (opfive) {
        int q = find_cell(b, op, P4_FIVE);
        if (opfive >= 2 || !legal(b, me, q)) return -(SCORE_WIN - ply - 1);
        board_move(b, q);
        if (ply + 1 < MAX_PLY) c->xhalf[ply + 1] = c->xhalf[ply];
        int v = -pvs(c, -beta, -alpha, depth, ply + 1, is_pv);
        board_undo(b);
        if (is_pv && !c->stop) {
            c->pv[ply][0] = q;
            memcpy(&c->pv[ply][1], c->pv[ply + 1], sizeof(int) * c->pvlen[ply + 1]);
            c->pvlen[ply] = c->pvlen[ply + 1] + 1;
        }
        return v;
    }
    // 自分の達四: 勝ち
    if (legal_flex4(b, me) >= 0) return SCORE_WIN - ply - 2;

    int op_threat = legal_flex4(b, op) >= 0;
    // 静止探索: 深さが尽きたら、相手の活三への防ぎ (脅威がある時) か自分の四だけを読む
    int qmode = depth <= 0, stand = -SCORE_INF;
    if (qmode) {
        // 末端の VCF: 評価の尺度を変えずに、手番側の四の連続による勝ちだけを見つける
        if (sp.leaf_vcf && depth == 0 &&
            b->p4cnt[me][P4_BLOCK4] + b->p4cnt[me][P4_BLOCK4_PLUS] + b->p4cnt[me][P4_BLOCK4_FLEX3] > 0) {
            TssResult r;
            if (g_fvcf ? fvcf_solve(b, me, sp.leaf_vcf_depth, sp.leaf_vcf, &r) : tss_solve(b, me, TSS_VCF, sp.leaf_vcf_depth, sp.leaf_vcf, &r))
                return SCORE_WIN - ply - r.nline;
        }
        if (depth <= -sp.qdepth) return sp.tpv == 1 ? tp_leaf_eval(b, evaluate(b), alpha, beta) : evaluate(b);
        if (!op_threat) {
            stand = sp.tpv == 1 ? tp_leaf_eval(b, evaluate(b), alpha, beta) : evaluate(b);
            if (stand >= beta) return stand;
            if (stand > alpha) alpha = stand;
        }
    }

    // 置換表
    int ttmove = -1;
    TTData e;
    if (tt_probe(b->hash, &e)) {
        ttmove = e.move;
        if (!is_pv && e.depth >= depth) {
            int v = value_from_tt(e.value, ply);
            if (e.flag == TT_EXACT || (e.flag == TT_LOWER && v >= beta) || (e.flag == TT_UPPER && v <= alpha))
                return v;
        }
    }

    // 静的評価による打ち切り (Rapfi 風、既定は無効): futility と razoring。手の数の枝刈りの「評価が良くなっているか」もここで決める
    int improving = 1;
    if (!qmode && (sp.mcp || sp.fut || sp.razor)) {
        int sev = evaluate(b);
        if (ply < MAX_PLY) c->seval[ply] = sev;
        improving = ply < 2 || sev > c->seval[ply - 2];
        if (!is_pv && !op_threat && beta < SCORE_WIN_MIN && alpha > -SCORE_WIN_MIN) {
            int m = depth - 1 - improving;
            if (sp.fut && depth <= sp.fut_depth && sev - sp.fut_margin * (m > 0 ? m : 0) >= beta) return sev;
            if (sp.razor && depth <= sp.razor_depth && sev + sp.razor_margin * depth < alpha) return pvs(c, alpha, alpha + 1, 0, ply, 0);
        }
    }

    // 勝勢の側の脅威探索 (ts.c): 評価が高く脅威の手がある局面で、脅威の手だけで勝てるかを短い予算で調べる
    if (sp.ts_tree && !qmode && !op_threat && depth >= sp.ts_tree_depth && (!sp.ts_tree_side || (ply & 1)) && (!sp.ts_tree_pv || is_pv) && tstree_gate &&
        b->p4cnt[me][P4_FLEX3] + b->p4cnt[me][P4_FLEX3_PLUS] + b->p4cnt[me][P4_FLEX3_2X] + b->p4cnt[me][P4_BLOCK4_FLEX3] +
        b->p4cnt[me][P4_BLOCK4] + b->p4cnt[me][P4_BLOCK4_PLUS] > 0 && evaluate(b) >= sp.ts_tree_eval) {
        TsResult tr;
        // 同じ予算で勝てなかった局面は覚えておき、反復深化の次の回で呼び直さない
        uint64_t fk = b->hash ^ (uint64_t)sp.ts_tree * 0x9e3779b97f4a7c15ull ^ (uint64_t)sp.ts_tree_plies << 58;
        uint64_t *fe = &tsfail[fk & (TSFAIL_SIZE - 1)];
        int w = 0;
        tr.nodes = 0;
        if (sp.ts_tree_fail && *fe == fk) tstree_stat[11]++;
        else {
            tstree_stat[0]++;
            w = ts_probe(b, me, sp.ts_tree_plies, sp.ts_tree, &tr);
            if (!w && sp.ts_tree_fail) *fe = fk;
        }
        tstree_stat[1] += tr.nodes;
        tstree_stat[2 + (depth < 7 ? depth : 7)]++;
        if (w) {
            tstree_stat[10]++;
            int v = SCORE_WIN - ply - 2 * tr.depth - 1;
            // 証明した勝ちは置換表に下限として残す (反復深化の次の回で ts を呼び直さない)
            if (sp.ts_tree_tt) tt_store(b->hash, v, tr.move, 100, TT_LOWER, ply);
            return v;
        }
    }
    // 脅威圏: パスしたら相手に VCF があるなら、その手順を崩し得る点と自分の四だけを読む (他は同じ手順で負け)
    // 手番側の VCF (本探索の深さでは届かない長い四の連続)
    if (sp.own_vcf && !qmode && depth >= sp.own_vcf && tz_gate(b, me)) {
        TzZone z;
        if (tz_probe_att(b, me, sp.tz_nodes, &z)) return SCORE_WIN - ply - z.plies;
    }
    int tzon = 0, tzplies = 0, tzres = -1;   // tzres: -1 = 調べていない、0 = 相手に VCF なし、1/2 = あり
    int want_nmp = sp.nmp && !is_pv && !qmode && !op_threat && !b->passes && depth >= sp.nmp_depth &&
                   beta < SCORE_WIN_MIN && beta > -SCORE_WIN_MIN;
    if (!qmode && !op_threat && ((sp.tz && depth >= sp.tz_depth) || want_nmp)) {
        if (!tz_gate(b, op)) tzres = 0;
        else {
            TzZone z;
            tzres = tz_probe(b, sp.tz_nodes, &z);
            if (tzres == 1 && sp.tz && depth >= sp.tz_depth) {
                tzon = 1;
                tzplies = z.plies;
                c->tzgen++;
                for (int i = 0; i < z.n; i++) c->tzmark[z.cells[i]] = c->tzgen;
            }
        }
    }
    // null move: 五目並べは石が増えて損をしない (ツークツワンクがない) ので、パスしても beta 以上なら打ち切ってよい。
    // パスした後に相手の VCF で負ける局面 (読みの深さが足りず誤る典型) は脅威圏の判定で除く
    if (want_nmp) nmp_stat[0]++;
    if (want_nmp && tzres == 0) nmp_stat[1]++;
    if (want_nmp && tzres == 0 && evaluate(b) >= beta) {
        nmp_stat[2]++;
        int r = sp.nmp_r + depth / 6;
        board_pass(b);
        int v = -pvs(c, -beta, -beta + 1, depth - 1 - r, ply + 1, 0);
        board_pass(b);
        if (c->stop) return 0;
        if (v >= beta) { nmp_stat[3]++; return v >= SCORE_WIN_MIN ? beta : v; }
    }

    // 相手の活三を受ける節点: 自分に VCF があれば勝ち。なければ、受けにならない自分のただの四 (四三でなく、
    // 自分の石が相手の四の線に乗らない) は、相手が止めた後に同じ活三が残るだけなので読まない
    c->drop4 = 0;
    if (op_threat && !sp.def_own4 && depth <= sp.def_own4_depth) {
        if (b->p4cnt[me][P4_BLOCK4] + b->p4cnt[me][P4_BLOCK4_PLUS] >= sp.def_vcf_min) {
            TssResult vr;
            if (fvcf_solve(b, me, sp.def_vcf_depth, sp.def_vcf_nodes, &vr)) return SCORE_WIN - ply - vr.nline;
        }
        c->drop4 = 1;
    }
    // 相手の VCT の圏 (relevance zone): パスしたら相手に VCT があるなら、その証明の圏の外の手 (反撃の形を作る手を除く) は
    // 同じ手順で負けるので読まない。手の並べ方に頼らない枝刈り
    int vzon = sp.vz && !qmode && !op_threat && !tzon && depth >= sp.vz_depth && (sp.vz_pv || !is_pv) &&
               alpha > -SCORE_WIN_MIN && vz_gate(b, op) && vz_mark(c, b, op, sp.vz_nodes);
    Move list[MAX_MOVES];
    int n, i0 = 0, ttstage = 0, gmode = qmode && !op_threat, drop4 = c->drop4;
    if (sp.ttfirst && ttmove >= 0 && !tzon && !vzon && !sp.tpo && !sp.mcp && !(op_threat && sp.def_line) &&
        gen_ok(c, ttmove, me, op, op_threat, gmode)) {
        // 段階的な手の生成: 置換表の手だけを先に読み、切れなければ残りを生成する
        ttstage = 1;
        list[0].pos = ttmove;
        list[0].score = 1 << 28;
        list[0].value = 0;
        list[0].tactical = b->pat4[me][ttmove] >= sp.tact_own || b->pat4[op][ttmove] >= sp.tact_def;
        n = 1;
    } else
        n = gen_moves(c, list, me, op, op_threat, ttmove, ply, gmode, tzon);
    c->drop4 = 0;
    if (vzon) {
        int m = 0;
        vz_stat[3] += n;
        for (int i = 0; i < n; i++)
            if (c->tzmark[list[i].pos] == c->tzgen || vz_keep_move(b, me, list[i].pos)) list[m++] = list[i];
        n = m;
        vz_stat[4] += n;
    }
    // 脅威の潜在力 (tp.c): 手番側の λ2 の脅威の点と、相手の λ2 の脅威の点 (先にふさぐ) を先に読む
    if (sp.tpo && !qmode && !op_threat && !tzon && depth >= sp.tpo_depth && tp_prepare(b) > 0)
        for (int i = 0; i < n; i++) {
            int m = tp_mark(list[i].pos);
            if (!m || list[i].pos == ttmove) continue;
            list[i].score += (m & 1 ? sp.tpo_own : 0) + (m & 2 ? sp.tpo_opp : 0);
            if (sp.tpo == 2) list[i].tactical = 1;
        }
    int best = stand, bestmove = -1, alpha0 = alpha, searched = 0;
    int rzon = sp.rz && !is_pv && !qmode && depth >= sp.rz_depth && !op_threat && !tzon;
    if (rzon) rz_reset(ply);
    // 葉の手前の戦術的な手の futility: 評価が alpha よりずっと低いなら、ただの三とその受けで alpha を超えることはまずない
    // (残り深さ 1、stand - alpha < -600 で三が alpha を超えたのは 0.4〜2%)。四・四三・三三は残す
    int tfon = sp.tfut && depth == 1 && !is_pv && !op_threat && !qmode && alpha > -SCORE_WIN_MIN && alpha < SCORE_WIN_MIN &&
               evaluate(b) + sp.tfut_margin <= alpha;

    int pmax = 0, pmax_n = -1;   // lmr_pol: 候補手の点の最大 (段階的な生成で n が増えたら取り直す)

moves_loop:
    for (int i = i0; i < n; i++) {
        pick_move(list, i, n);
        int q = list[i].pos, tactical = list[i].tactical || op_threat || tzon || (vzon && sp.vz_tact);
        // 全部の深さの手の数の枝刈り (Rapfi 風、既定は無効)。i + 1 = 何手目か (枝刈りした手も数える)
        if (sp.mcp && !qmode && searched > 0 && best > -SCORE_WIN_MIN && (sp.mcp_tact || !tactical) && (sp.mcp_pv || !is_pv) &&
            (i + 1) * (2 - improving) >= mcp_table[depth < 63 ? depth : 63])
            continue;
        if (searched > 0 && !is_pv && !tactical && best > -SCORE_WIN_MIN) {
            if (depth >= 1 && depth <= 4 && searched * 100 >= LMP_COUNT[depth] * sp.lmp_mul) continue;
            if (list[i].value < sp.cand_min) continue;
        }
        if (tfon && searched > 0 && best > -SCORE_WIN_MIN) {
            int a = b->pat4[me][q];
            if (a < P4_BLOCK4 && a != P4_FLEX3_2X && (sp.tfut >= 2 || a >= P4_FLEX3 || !tactical)) continue;
        }
        if (restricted(b, me) && board_is_forbidden(b, q)) continue;
        int rzout = rzon && !tactical && searched >= sp.rz_after && rz_count(ply) > 0 && best > -SCORE_WIN_MIN &&
                    !rz_relevant(ply, q, sp.rz_rad, sp.rz_lines);
        if (rzout && sp.rz == 2) continue;

        int a4 = b->pat4[me][q];
        board_move(b, q);
        int v, nd = depth - 1;
        // 活三を作る手 (相手は受けるしかない) の延長
        int ext_ok = 2 * (ply + depth) < sp.ext_limit * c->root_depth;
        int carry = ply < MAX_PLY - 1 ? c->xhalf[ply] : 0;   // ext_three == 2: 半手分の延長の繰り越し
        if (sp.ext_three == 1 && !qmode && a4 >= P4_FLEX3 && a4 <= P4_FLEX3_2X && ext_ok) nd = depth;
        // ext_three == 2: 活三を作る手を半手だけ延長する (2 手で 1 手分。繰り越しを子に渡す)
        if (sp.ext_three == 2 && !qmode && a4 >= P4_FLEX3 && a4 <= P4_FLEX3_2X && ext_ok) {
            if (carry) carry = 0;
            else nd = depth, carry = 1;
        }
        if (ply + 1 < MAX_PLY) c->xhalf[ply + 1] = (uint8_t)carry;
        if (sp.ext_four && !qmode && a4 >= P4_BLOCK4 && a4 <= P4_BLOCK4_FLEX3 && ext_ok) nd = depth;
        if (tzon && sp.tz_ext && ext_ok) nd = depth;
        if (searched == 0) {
            v = -pvs(c, -beta, -alpha, nd, ply + 1, is_pv);
        } else {
            int r = 0;
            if (!qmode && depth >= 3 && searched >= sp.lmr_moves && !tactical) {
                if (sp.lmr_log) {
                    r = lmr_table[depth < 63 ? depth : 63][searched < 63 ? searched : 63];
                    if (is_pv) r = r * sp.lmr_pv / 100;
                    if (r < 1) r = 1;
                } else {
                    r = 1 + (searched >= 10) + (depth >= 8 && searched >= 20);
                }
                if (sp.lmr_pol && sp.policy) {
                    // 手の並べ方の点が最大の手より lmr_pol (100 ≒ 1 logit) 低いごとに 1 手多く削る (Rapfi 風)
                    if (pmax_n != n) {
                        pmax = -(1 << 30);
                        for (int j = 0; j < n; j++)   // (段階的な生成で先に置いた置換表の手は点がないので除く)
                            if (list[j].score < (1 << 28) && list[j].value > pmax) pmax = list[j].value;
                        if (pmax == -(1 << 30)) pmax = list[i].value;
                        pmax_n = n;
                    }
                    int extra = (pmax - list[i].value) / sp.lmr_pol;
                    r += extra < sp.lmr_pol_max ? extra : sp.lmr_pol_max;
                }
                if (r > nd - 1) r = nd - 1 > 0 ? nd - 1 : 0;
            }
            if (sp.lmr_tact && tactical && !r && !qmode && depth >= 3 && !op_threat && !tzon && searched >= sp.lmr_tact &&
                a4 < P4_BLOCK4 && a4 != P4_FLEX3_2X && nd >= 2)
                r = 1;
            if (rzout) {
                r += sp.rz_red;
                if (r > nd - 1) r = nd - 1 > 0 ? nd - 1 : 0;
            }
            v = -pvs(c, -alpha - 1, -alpha, nd - r, ply + 1, 0);
            if (v > alpha && r) v = -pvs(c, -alpha - 1, -alpha, nd, ply + 1, 0);
            if (v > alpha && v < beta) v = -pvs(c, -beta, -alpha, nd, ply + 1, 1);
        }
        if (rzon && v <= alpha && !c->stop) {
            // 反証の読み筋: 置換表の最善手を rz_len 手たどる
            int k = 0;
            for (; k < sp.rz_len; k++) {
                TTData te;
                if (!tt_probe(b->hash, &te) || te.move < 0 || b->cell[te.move] != EMPTY) break;
                rz_add(ply, te.move);
                board_move(b, te.move);
            }
            while (k-- > 0) board_undo(b);
        }
        board_undo(b);
        if (c->stop) return 0;
        searched++;

        if (v > best) {
            best = v;
            bestmove = q;
            if (v > alpha) {
                alpha = v;
                if (is_pv) {
                    c->pv[ply][0] = q;
                    memcpy(&c->pv[ply][1], c->pv[ply + 1], sizeof(int) * c->pvlen[ply + 1]);
                    c->pvlen[ply] = c->pvlen[ply + 1] + 1;
                }
                if (v >= beta) {
                    if (!list[i].tactical && ply < MAX_PLY && c->killer[ply][0] != q) {
                        c->killer[ply][1] = c->killer[ply][0];
                        c->killer[ply][0] = q;
                    }
                    int *h = &c->history[me][q];
                    *h += depth * depth * 32;
                    if (*h > (1 << 20)) {
                        for (int cc = 1; cc <= 2; cc++)
                            for (int p = 0; p < NCELL; p++) c->history[cc][p] /= 2;
                    }
                    break;
                }
            }
        }
    }

    if (ttstage == 1 && best < beta) {
        // 置換表の手で切れなかった: 残りを生成する (置換表の手を先頭に置いて、その次から読む)
        ttstage = 2;
        c->drop4 = drop4;
        n = gen_moves(c, list, me, op, op_threat, ttmove, ply, gmode, tzon);
        c->drop4 = 0;
        i0 = 0;
        for (int k = 0; k < n; k++)
            if (list[k].pos == ttmove) {
                Move t = list[0];
                list[0] = list[k];
                list[k] = t;
                i0 = 1;
                break;
            }
        goto moves_loop;
    }
    if (searched == 0) {
        // 打てる手がない: 相手の活三を止められない (負け)、または全部禁じ手 / 静止探索で四がない
        if (op_threat) best = -(SCORE_WIN - ply - 3);
        else if (tzon) best = -(SCORE_WIN - ply - tzplies);
        else if (vzon) best = -(SCORE_WIN - ply - 30);
        else if (qmode) best = stand;
        else best = evaluate(b);
    }

    int flag = best >= beta ? TT_LOWER : (best > alpha0 ? TT_EXACT : TT_UPPER);
    tt_store(b->hash, best, bestmove, depth, flag, ply);
    return best;
}

// ---------------------------------------------------------------- ルート

typedef struct {
    int pos;
    int score;      // 前の反復の点 (並べ替え用)
} RootMove;

#define ASPIRATION 60

static void log_info(const Ctx *c, int depth, int score, const int *pv, int pvlen, int verbose) {
    if (!verbose) return;
    double ms = now_ms() - c->t0;
    fprintf(stderr, "depth %2d sel %2d score %6d nodes %9ld nps %8.0f time %6.0f pv", depth, c->seldepth, score, c->nodes,
            ms > 0 ? c->nodes / ms * 1000 : 0, ms);
    for (int i = 0; i < pvlen && i < 12; i++) fprintf(stderr, " %d,%d", pos_x(pv[i]), pos_y(pv[i]));
    fprintf(stderr, "\n");
}

// Lazy SMP: 補助スレッドも同じ反復深化を (深さを少しずらして) 回し、置換表を通じて主スレッドを助ける
static const int SKIP_SIZE[20] = {1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4};
static const int SKIP_PHASE[20] = {0, 1, 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 7};

static void ctx_reset(Ctx *c, Board *b, int id, double t0) {
    c->b = b;
    c->id = id;
    c->nodes = 0;
    c->max_nodes = 0;
    c->stop = 0;
    c->t0 = t0;
    c->hard_ms = 1 << 30;
    c->seldepth = 0;
    c->root_depth = 0;
    memset(c->killer, 0, sizeof c->killer);
    memset(c->pvlen, 0, sizeof c->pvlen);
    // history は前の手の情報を少し残す
    for (int cc = 1; cc <= 2; cc++)
        for (int p = 0; p < NCELL; p++) c->history[cc][p] /= 4;
}

// 反復深化 (ルートの候補 root[0..nr) を調べ、結果を res に入れる)
static void iterate(Ctx *c, RootMove *root, int nr, const SearchLimits *lim, SearchResult *res) {
    Board *b = c->b;
    int max_depth = lim->max_depth > 0 ? lim->max_depth : MAX_PLY - 10;
    int prev_score = 0, prev_best = -1, unstable = 0;
    for (int depth = 1; depth <= max_depth; depth++) {
        if (c->id > 0) {
            // 補助スレッドはスレッドごとに一部の深さを飛ばす (同じ深さを全員で読まないように)
            int k = (c->id - 1) % 20;
            if ((depth + SKIP_PHASE[k]) / SKIP_SIZE[k] % 2) continue;
        }
        c->root_depth = depth;
        // 本探索の中の ts を、根の手番側が前の反復で -ts_tree_root 以下 (相手が押している) のときだけ使う
        tstree_gate = !sp.ts_tree_root || (depth > 1 && prev_score <= -sp.ts_tree_root);
        // aspiration window: 前の反復の点の周りの狭い窓で調べ、外れたら広げてやり直す
        int lo = -SCORE_INF, hi = SCORE_INF, best, bestmove, searched;
        int bestpv[MAX_PLY], bestpvlen;
        if (depth >= 4 && prev_score > -SCORE_WIN_MIN && prev_score < SCORE_WIN_MIN) {
            lo = prev_score - ASPIRATION;
            hi = prev_score + ASPIRATION;
        }
        for (;;) {
            int alpha = lo, beta = hi;
            best = -SCORE_INF, bestmove = -1, searched = 0, bestpvlen = 0;
            for (int i = 0; i < nr; i++) {
                int p = root[i].pos;
                board_move(b, p);
                int v;
                if (searched == 0) {
                    v = -pvs(c, -beta, -alpha, depth - 1, 1, 1);
                } else {
                    v = -pvs(c, -alpha - 1, -alpha, depth - 1, 1, 0);
                    if (v > alpha && v < beta && !c->stop) v = -pvs(c, -beta, -alpha, depth - 1, 1, 1);
                }
                board_undo(b);
                if (c->stop) break;
                searched++;
                root[i].score = v;
                if (v > best) {
                    best = v;
                    bestmove = p;
                    if (v > alpha) alpha = v;
                    bestpv[0] = p;
                    memcpy(&bestpv[1], c->pv[1], sizeof(int) * c->pvlen[1]);
                    bestpvlen = c->pvlen[1] + 1;
                    if (v > lo || searched == 1) {
                        // 反復の途中で打ち切られても、全幅で調べてより良かった手は使える
                        res->move = p;
                        res->score = v;
                        res->pvlen = bestpvlen;
                        memcpy(res->pv, bestpv, sizeof(int) * bestpvlen);
                    }
                    if (v >= beta) break;
                } else {
                    root[i].score = -SCORE_INF - i;   // null window で落ちた手は順位だけ
                }
            }
            if (c->stop) break;
            if (best <= lo && lo > -SCORE_INF) { lo = -SCORE_INF; continue; }
            if (best >= hi && hi < SCORE_INF) { hi = SCORE_INF; continue; }
            break;
        }
        if (c->stop && searched > 0 && best < -SCORE_WIN_MIN && searched < nr) {
            // 打ち切り時に、調べ終えた手が全部負けと分かっただけなら、まだ調べていない次の候補を打つ
            res->move = root[searched].pos;
            res->score = prev_score;
        }
        if (c->stop && searched == 0) break;
        if (!c->stop) {
            unstable = depth > 1 && bestmove != prev_best;
            prev_best = bestmove;
            res->depth = depth;
            prev_score = best;
            if (c->id == 0) log_info(c, depth, best, bestpv, bestpvlen, lim->verbose);
            // 補助スレッドが勝ちを読み切ったら全員止める (その手を主スレッドが使う)
            if (sp.smp_pick && c->id > 0 && best > SCORE_WIN_MIN) __atomic_store_n(&g_stop, 1, __ATOMIC_RELAXED);
        }
        // 次の反復のために並べ替え (最善手を先頭、他は点の順。安定ソート)
        for (int i = 1; i < nr; i++) {
            RootMove m = root[i];
            int j = i - 1;
            while (j >= 0 && root[j].score < m.score) { root[j + 1] = root[j]; j--; }
            root[j + 1] = m;
        }
        if (c->stop) break;
        if (best > SCORE_WIN_MIN || best < -SCORE_WIN_MIN) {
            // 詰みが見えた: 少し深く確かめたら終わる
            if (depth >= 2 + (SCORE_WIN - (best > 0 ? best : -best))) break;
        }
        if (lim->soft_ms > 0 && now_ms() - c->t0 >= (double)lim->soft_ms * (unstable ? sp.unstable_pct : 100) / 100) break;
    }
}

typedef struct {
    Ctx *c;
    RootMove root[MAX_MOVES];
    int nr;
    SearchLimits lim;
    SearchResult res;
} Helper;

static Helper helpers[MAX_THREADS];

static void *helper_main(void *arg) {
    Helper *h = arg;
    tss_set_thread(h->c->id);
    fvcf_set_thread(h->c->id);
    iterate(h->c, h->root, h->nr, &h->lim, &h->res);
    return NULL;
}

// 相手の VCT への受け (df-pn): 相手がパスしたら VCT がある局面なら、ルートの候補を順に打って相手の VCT が
// 証明された手 (負けが確定) を外す。時間が尽きて調べられなかった手・証明できなかった手は残す。全部負けなら外さない。
// 時間は上限時間の dfdef_pct % (その 1/4 で相手の VCT の有無を調べる)
static void dfpn_defenses(Ctx *c, Board *b, int op, RootMove *root, int *nr, long hard, int verbose) {
    double t_end = c->t0 + hard * sp.dfdef_pct / 100.0;
    DfpnResult r;
    int budget0 = (int)(hard * sp.dfdef_pct / 400);
    if (budget0 < 1 || !dfpn_vct(b, op, 1L << 40, budget0, &r)) return;
    RootMove keep[MAX_MOVES];
    int nk = 0, lost = 0, n = *nr;
    for (int i = 0; i < n; i++) {
        double left = t_end - now_ms();
        int per = (int)(left / (n - i < 8 ? n - i : 8));
        if (per >= 1) {
            board_move(b, root[i].pos);
            int lose = dfpn_vct(b, op, 1L << 40, per, &r);
            board_undo(b);
            if (lose) { lost++; continue; }
        }
        keep[nk++] = root[i];
    }
    if (verbose) fprintf(stderr, "opponent has VCT: %d / %d root moves proven lost\n", lost, n);
    if (nk > 0 && lost > 0) {
        memcpy(root, keep, sizeof(RootMove) * nk);
        *nr = nk;
    }
}

// 探索で選んだ手を打った後に相手の VCT がないかを df-pn で確かめ、負けが証明されたら次の候補 (前の反復の点の順) を
// 確かめる。残り時間の半分ずつを使い、証明できなかった手 (勝ちがない・打ち切り) を打つ。全部負けなら元の手のまま
static void verify_move(Ctx *c, Board *b, int op, RootMove *root, int nr, int hard_ms, SearchResult *res, int verbose) {
    int cand[MAX_MOVES], nc = 0;
    cand[nc++] = res->move;
    for (int i = 0; i < nr; i++)
        if (root[i].pos != res->move) cand[nc++] = root[i].pos;
    for (int i = 0; i < nc; i++) {
        double left = c->t0 + hard_ms - now_ms();
        int per = (int)(i + 1 < nc ? left / 2 : left);
        if (per < 2) return;
        DfpnResult r;
        board_move(b, cand[i]);
        int lose;
        if (sp.dfver_ts) {
            TsResult tr;
            lose = 0;
            r.nodes = 0;
            r.aborted = 0;
            if (sp.dfver_ts == 1) lose = dfpn_vct(b, op, 1L << 40, per / 2, &r);
            if (!lose) {
                lose = ts_win(b, op, 40, 1L << 40, sp.dfver_ts == 1 ? per / 2 : per, &tr);
                r.nodes += tr.nodes;
                r.aborted = tr.aborted;
            }
        } else if (sp.dfver_l3) {
            L3Result lr;
            lose = l3_win(b, op, 1L << 40, per, &lr);
            r.nodes = lr.nodes;
            r.aborted = lr.aborted;
        } else
            lose = dfpn_vct(b, op, 1L << 40, per, &r);
        board_undo(b);
        if (verbose) fprintf(stderr, "verify %d,%d: %s (%ld nodes)\n", pos_x(cand[i]), pos_y(cand[i]), lose ? "opponent VCT" : r.aborted ? "unknown" : "safe", r.nodes);
        if (!lose) {
            if (i > 0) {
                res->move = cand[i];
                res->pvlen = 0;
                res->reason = "verified";
            }
            return;
        }
    }
}

void search_root(Board *b, const SearchLimits *lim, SearchResult *res) {
    Ctx *c = &ctxs[0];
    if (!lmr_table_ok) lmr_init();
    if (!pol_ok || pol_rule != b->rule.forbid_33 * 4 + b->rule.forbid_44 * 2 + b->rule.forbid_overline) {
        policy_init();
        pol_rule = b->rule.forbid_33 * 4 + b->rule.forbid_44 * 2 + b->rule.forbid_overline;
    }
    tt_alloc();
    tt_used = 1;
    tt_gen++;
    __atomic_store_n(&g_stop, 0, __ATOMIC_RELAXED);
    double t0 = now_ms();
    ctx_reset(c, b, 0, t0);
    c->hard_ms = lim->hard_ms > 0 ? lim->hard_ms : 1 << 30;
    // 選んだ手の確かめ (dfver_pct) の時間を探索の上限から取っておく
    if (sp.dfver_pct > 0 && lim->hard_ms > 0) c->hard_ms = lim->hard_ms * (100 - sp.dfver_pct) / 100;
    c->max_nodes = lim->max_nodes;
    memset(res, 0, sizeof *res);
    res->move = -1;
    res->reason = "search";

    int me = board_side(b), op = opp_color(me);
    if (b->nmoves == 0) {
        res->move = to_pos(b->size / 2, b->size / 2);
        res->reason = "center";
        return;
    }
    // 即決の手
    int q = find_cell(b, me, P4_FIVE);
    if (q >= 0) { res->move = q; res->score = SCORE_WIN; res->reason = "five"; return; }
    if (b->p4cnt[op][P4_FIVE]) {
        q = find_cell(b, op, P4_FIVE);
        res->move = q;
        res->score = b->p4cnt[op][P4_FIVE] >= 2 || !legal(b, me, q) ? -SCORE_WIN : 0;
        res->reason = "block-five";
        if (b->p4cnt[op][P4_FIVE] >= 2 || legal(b, me, q)) {
            // 1 つなら止めるしかない。2 つ以上なら負け (どちらかを止める)
            return;
        }
        // 黒が止める点が禁じ手: 負けだが他の手を探す (下へ)
    }
    q = legal_flex4(b, me);
    if (q >= 0) { res->move = q; res->score = SCORE_WIN - 2; res->reason = "flex4"; return; }

    // 詰み探索 (vcf.c): 自分の VCF → 相手の VCF への受け → 自分の VCT
    // ノード数は持ち時間に比例させる (VCF・VCT の 1 ノードはおよそ 1.5us)
    long hard = lim->hard_ms > 0 ? lim->hard_ms : 1000;
    int vmove;
    TssResult vres;
    int vwin = g_fvcf ? fvcf_solve(b, board_side(b), 40, hard * 60, &vres) : tss_solve(b, board_side(b), TSS_VCF, 40, hard * 60, &vres);
    vmove = vres.move;
    if (vwin) {
        res->move = vmove;
        res->score = SCORE_WIN - 10;
        res->reason = "vcf";
        return;
    }
    static int defs[MAX_MOVES];
    int ndefs = vcf_defenses(b, hard * 60, defs);   // -1 = 相手に VCF なし
    // df-pn は上限時間の vct_pct % まで
    DfpnResult dr;
    if (ndefs < 0 && sp.vct_pct > 0 &&
        (sp.vct_dfpn ? (dfpn_vct(b, me, 1L << 40, (int)(hard * sp.vct_pct / 100), &dr) && (vmove = dr.move) >= 0)
                     : vct_solve(b, sp.vct_depth, hard * sp.vct_pct * 6, &vmove))) {
        res->move = vmove;
        res->score = SCORE_WIN - 20;
        res->reason = "vct";
        return;
    }
    TsResult tsr;
    if (ndefs < 0 && sp.ts_pct > 0 && ts_win(b, me, 40, 1L << 40, (int)(hard * sp.ts_pct / 100), &tsr) && tsr.move >= 0) {
        res->move = tsr.move;
        res->score = SCORE_WIN - 20;
        res->reason = "ts";
        return;
    }
    if (ndefs < 0 && sp.vctdef_pct > 0) {
        // 相手に VCT があれば、それを崩し得る手 (返されなかった手は負けと証明済み) に絞る
        ndefs = vct_defenses(b, sp.vct_depth, hard * sp.vctdef_pct * 6, defs);
        if (lim->verbose && ndefs >= 0) fprintf(stderr, "opponent has VCT: %d defenses\n", ndefs);
    }
    if (lim->verbose && ndefs >= 0) fprintf(stderr, "opponent has VCF: %d defenses\n", ndefs);

    // ルートの候補
    int op_threat = legal_flex4(b, op) >= 0;
    Move list[MAX_MOVES];
    TTData e;
    int n = gen_moves(c, list, me, op, op_threat, tt_probe(b->hash, &e) ? e.move : -1, 0, 0, 0);
    sort_moves(list, n);
    RootMove root[MAX_MOVES];
    int nr = 0;
    // 相手の VCF への受け (vcf_defenses、受けになり得る手の上位集合) を先に調べる。def_restrict なら受けだけに絞る
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < n; i++) {
            int p = list[i].pos, isdef = 0;
            for (int k = 0; k < ndefs && !isdef; k++) isdef = defs[k] == p;
            if ((ndefs <= 0 || isdef == (pass == 0)) && legal(b, me, p)) root[nr++] = (RootMove){p, 0};
        }
        if (ndefs <= 0 || sp.def_restrict) break;
    }
    if (nr == 0 && ndefs > 0) {
        // 受けが候補の外 (遠い点) にしかない
        for (int k = 0; k < ndefs; k++)
            if (legal(b, me, defs[k])) root[nr++] = (RootMove){defs[k], 0};
    }
    if (nr == 0) {
        // 防ぎ手がない (相手の活三を止められない) か全部禁じ手: 何でもよいので合法手
        n = gen_moves(c, list, me, op, 0, -1, 0, 0, 0);
        sort_moves(list, n);
        for (int i = 0; i < n; i++)
            if (legal(b, me, list[i].pos)) root[nr++] = (RootMove){list[i].pos, 0};
        if (nr == 0) {
            for (int y = 0; y < b->size && res->move < 0; y++)
                for (int x = 0; x < b->size; x++) {
                    int p = to_pos(x, y);
                    if (b->cell[p] == EMPTY && legal(b, me, p)) { res->move = p; break; }
                }
            // 空点が禁じ手しかない: エラーにせず空点を返す (禁じ手を打って負け。返さないと対局が壊れる)
            for (int y = 0; y < b->size && res->move < 0; y++)
                for (int x = 0; x < b->size; x++)
                    if (b->cell[to_pos(x, y)] == EMPTY) { res->move = to_pos(x, y); break; }
            res->reason = "no-move";
            return;
        }
        res->move = root[0].pos;
        res->score = -SCORE_WIN;
        res->reason = "lost";
        return;
    }
    if (sp.vz_root && ndefs < 0 && nr > 1 && !op_threat && vz_mark(c, b, op, sp.vz_root_nodes)) {
        // ルート: 相手の VCT の圏の外の手 (反撃の形を作る手を除く) を外す
        int m = 0;
        for (int i = 0; i < nr; i++)
            if (c->tzmark[root[i].pos] == c->tzgen || vz_keep_move(b, me, root[i].pos)) root[m++] = root[i];
        if (lim->verbose) fprintf(stderr, "opponent VCT zone: %d / %d root moves kept\n", m, nr);
        if (m > 0) { vz_stat[5] += nr - m; nr = m; }
    }
    if (sp.dfdef_pct > 0 && ndefs < 0 && nr > 1) dfpn_defenses(c, b, op, root, &nr, hard, lim->verbose);
    res->move = root[0].pos;
    if (nr == 1) { res->reason = "only-move"; return; }

    pthread_t th[MAX_THREADS];
    int started[MAX_THREADS] = {0};
    for (int t = 1; t < n_threads; t++) {
        Helper *h = &helpers[t];
        boards[t] = *b;
        h->c = &ctxs[t];
        ctx_reset(h->c, &boards[t], t, t0);
        memcpy(h->root, root, sizeof(RootMove) * nr);
        h->nr = nr;
        h->lim = *lim;
        h->lim.soft_ms = 0;
        h->lim.verbose = 0;
        memset(&h->res, 0, sizeof h->res);
        h->res.move = -1;
        started[t] = pthread_create(&th[t], NULL, helper_main, h) == 0;
    }
    iterate(c, root, nr, lim, res);
    __atomic_store_n(&g_stop, 1, __ATOMIC_RELAXED);
    res->nodes = c->nodes;
    for (int t = 1; t < n_threads; t++) {
        if (!started[t]) continue;
        pthread_join(th[t], NULL);
        res->nodes += ctxs[t].nodes;
        // 補助スレッドの結果を使う: 勝ちを読み切った、または主スレッドより深い反復を終えて点が下がらない
        const SearchResult *hr = &helpers[t].res;
        if (!sp.smp_pick || hr->depth <= 0 || hr->move < 0) continue;
        int win = hr->score > SCORE_WIN_MIN && res->score <= SCORE_WIN_MIN;
        int deeper = hr->depth > res->depth && hr->score >= res->score && res->score <= SCORE_WIN_MIN;
        if (win || deeper) {
            if (lim->verbose) fprintf(stderr, "thread %d: depth %d score %d move %d,%d\n", t, hr->depth, hr->score, pos_x(hr->move), pos_y(hr->move));
            res->move = hr->move;
            res->score = hr->score;
            res->depth = hr->depth;
            res->pvlen = hr->pvlen;
            memcpy(res->pv, hr->pv, sizeof(int) * hr->pvlen);
        }
    }
    res->seldepth = c->seldepth;
    if (sp.dfver_pct > 0 && lim->hard_ms > 0 && res->move >= 0 && res->score < SCORE_WIN_MIN)
        verify_move(c, b, op, root, nr, lim->hard_ms, res, lim->verbose);
    res->ms = (int)(now_ms() - c->t0);
}
