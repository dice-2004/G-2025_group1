// 証明数探索 (df-pn) による VCT (説明は dfpn.h)
#include "dfpn.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tz.h"

long g_dfpn_vcf_nodes = 100;   // 長い VCF はルートの vcf_solve が別に調べる。小さいほうが同じ時間で詰みを多く見つける
int g_dfpn_zone_k = 2;
int g_dfpn_flags = 0;
int g_dfpn_zone_union = 1;   // 1 = dfpn_vct_zone で達四の点ごとの圏の和を取る (0 = 前の共通部分だけ)
int g_dfpn_zone_def = P4_BLOCK3;   // 0 = 使わない。圏に入れる受け側の形の下限
int g_dfpn_l2 = 0;      // 1 = 三でない手でも、打った後にパスしたら VCF がある手 (VCF の脅威) を攻めに入れる   // 調査用: 1 = OR の VCF の確認をしない、2 = AND の受け側の VCF の確認をしない

#define INF 100000000u
#define DTT_BITS 19
#define DTT_SIZE (1u << DTT_BITS)
#define MAX_CH 96
#define MAX_DEPTH 90

typedef struct {
    uint64_t key;
    uint32_t pn, dn;
    int16_t best;
    uint16_t gen;
} DEntry;

static _Thread_local struct {
    DEntry *tt;
    uint16_t gen;
    Board *b;
    int att, def, forb;
    int root_n;
    long nodes, max_nodes;
    double deadline;    // 0 = 時間の制限なし
    int aborted;
    TzZone zone;        // 最後の gen_and の攻め側の脅威の圏 (zone_all = 絞れない)
    int zone_all;
    uint64_t root_hash; // 最後の dfpn_vct の根の局面 (dfpn_vct_resume で続きから調べるため)
    int or_kind;        // 直前の gen_or の勝ちの種類: 1 = 五、2 = 達四、3 = VCF (or_z、or_zr = tz_probe_att の戻り値)
    int or_zr;
    TzZone or_z;
    int shadow;         // 影の石 (dfpn_vct_shadow): この点の受け側の石を除いた局面の証明を、圏の外なら使う。-1 = なし
    uint64_t shadow_z;  // その石のハッシュ
    long shadow_hits;
} D;

// ---- 証明の圏の表 (証明ごとの圏。1 石違いの局面に証明を使い回すため) ----
// 証明した局面ごとに「受け側の石がこの外に 1 つ増えても証明は (ほぼ) 変わらない」点の集合を覚える (dfpn_vct_zone と同じ考え方を、
// 証明のたびに子の圏の和として下から作る)。w が全部 1 = 絞れない
#define ZW 7     // 20 路 × 20 = 400 点
#ifndef ZT_BITS
#define ZT_BITS 19
#endif
typedef struct {
    uint64_t key;
    uint64_t w[ZW];
} ZEnt;
static _Thread_local ZEnt *ZT;
int g_dfpn_zt = 0;   // 0 = 圏の表を作らない (dfpn_vct_shadow は使い回しをしない)
long dfpn_zt_stat[4];   // 圏を作った回数、うち絞れない、影の石で使い回した回数

static inline int zidx(int q) { return pos_x(q) + pos_y(q) * MAX_SIZE; }
static inline void zm_set(uint64_t *w, int q) { int i = zidx(q); w[i >> 6] |= 1ull << (i & 63); }
static inline int zm_has(const uint64_t *w, int q) { int i = zidx(q); return (int)(w[i >> 6] >> (i & 63) & 1); }
static inline void zm_all(uint64_t *w) { memset(w, 0xff, sizeof(uint64_t) * ZW); }
static inline int zm_is_all(const uint64_t *w) { return w[0] == ~0ull && w[1] == ~0ull && w[ZW - 1] == ~0ull; }
static inline void zm_or(uint64_t *w, const uint64_t *v) { for (int i = 0; i < ZW; i++) w[i] |= v[i]; }
static const uint64_t *zt_get(uint64_t key) {
    ZEnt *z = &ZT[key & ((1u << ZT_BITS) - 1)];
    return z->key == key ? z->w : NULL;
}
static void zt_put(uint64_t key, const uint64_t *w) {
    ZEnt *z = &ZT[key & ((1u << ZT_BITS) - 1)];
    z->key = key;
    memcpy(z->w, w, sizeof z->w);
    dfpn_zt_stat[0]++;
    if (zm_is_all(w)) dfpn_zt_stat[1]++;
}

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec * 1e-6;
}

static const uint64_t DSALT[3] = {0, 0x243f6a8885a308d3ULL, 0x13198a2e03707344ULL};
// 受け側の手番の局面の鍵に混ぜる。パスで同じ石の配置が攻め側の手番にも受け側の手番にもなる (呼び出しをまたぐ置換表の取り違えを防ぐ)
#define ANDSALT 0xa4093822299f31d0ULL
static inline uint64_t dkey(const Board *b, int is_or) { return b->hash ^ DSALT[D.att] ^ (is_or ? 0 : ANDSALT); }

static inline int dlegal(int color, int q) {
    return !(D.forb && color == BLACK && board_is_forbidden(D.b, q));
}

// 影の石を除いた局面の証明 (圏の外なら、影の石があっても同じ手順で勝つ)
static DEntry *probe_shadow(uint64_t key) {
    uint64_t k2 = key ^ D.shadow_z;
    DEntry *e2 = &D.tt[k2 & (DTT_SIZE - 1)];
    if (e2->key != k2 || e2->pn != 0) return NULL;
    const uint64_t *w = zt_get(k2);
    if (!w || zm_has(w, D.shadow)) return NULL;
    D.shadow_hits++;
    return e2;
}

static DEntry *probe(uint64_t key) {
    DEntry *e = &D.tt[key & (DTT_SIZE - 1)];
    if (e->key != key || (e->gen != D.gen && e->pn != 0)) {   // 前の呼び出しの証明 (経路によらず正しい) だけ使う
        return D.shadow >= 0 && ZT ? probe_shadow(key) : NULL;
    }
    if (e->pn != 0 && e->dn != 0 && D.shadow >= 0 && ZT) {
        DEntry *s2 = probe_shadow(key);
        if (s2) return s2;
    }
    return e;
}

// 子の局面 (ckey) の証明の圏を w に足す。分からなければ 0
static int child_zone(uint64_t ckey, uint64_t *w) {
    const uint64_t *z = zt_get(ckey);
    if (!z && D.shadow >= 0) {
        z = zt_get(ckey ^ D.shadow_z);
        if (z && zm_has(z, D.shadow)) z = NULL;
    }
    if (!z) return 0;
    zm_or(w, z);
    return 1;
}

// どの局面でも: 受け側の石が 1 つ増えて受け側の四ができる点 (証明木の受けが変わり得る)。dfpn_vct_zone と同じ
static void zm_base(uint64_t *w) {
    const Board *b = D.b;
    if (!g_dfpn_zone_def) return;
    for (int y = 0; y < b->size; y++) {
        int q = to_pos(0, y);
        for (int x = 0; x < b->size; x++, q++)
            if (b->cell[q] == EMPTY && b->pat4[D.def][q] >= g_dfpn_zone_def && b->pat4[D.def][q] != P4_FLEX2_2X) zm_set(w, q);
    }
}

static void zm_fives(uint64_t *w, int color) {
    const Board *b = D.b;
    for (int y = 0; y < b->size; y++) {
        int q = to_pos(0, y);
        for (int x = 0; x < b->size; x++, q++)
            if (b->cell[q] == EMPTY && b->pat4[color][q] == P4_FIVE) zm_set(w, q);
    }
}

static void store(uint64_t key, uint32_t pn, uint32_t dn, int best) {
    DEntry *e = &D.tt[key & (DTT_SIZE - 1)];
    if (e->key == key && e->pn == 0 && pn != 0) return;   // 証明は上書きしない
    e->key = key;
    e->pn = pn;
    e->dn = dn;
    e->best = (int16_t)best;
    e->gen = D.gen;
}

static int find_p4(int color, int p4) {
    const Board *b = D.b;
    for (int y = 0; y < b->size; y++) {
        int q = to_pos(0, y);
        for (int x = 0; x < b->size; x++, q++)
            if (b->cell[q] == EMPTY && b->pat4[color][q] == p4) return q;
    }
    return -1;
}

static int near_cells(int *out) {
    const Board *b = D.b;
    int n = 0;
    for (int y = 0; y < b->size; y++) {
        int q = to_pos(0, y);
        for (int x = 0; x < b->size; x++, q++)
            if (b->cell[q] == EMPTY && b->near[q]) out[n++] = q;
    }
    return n;
}

static int cheb(int p, int q) {
    int dx = abs(pos_x(p) - pos_x(q)), dy = abs(pos_y(p) - pos_y(q));
    return dx > dy ? dx : dy;
}

// 三を探す範囲: 探索中に打った攻め側の直近 k 手の 4 方向 ±4 と周囲 2 マス
static int in_locality(int q) {
    const Board *b = D.b;
    if (g_dfpn_zone_k <= 0 || b->nmoves - D.root_n < 2) return 1;
    for (int k = 0, i = b->nmoves - 2; k < g_dfpn_zone_k && i >= D.root_n; k++, i -= 2) {
        int m = b->moves[i];
        int dx = pos_x(q) - pos_x(m), dy = pos_y(q) - pos_y(m);
        int ax = abs(dx), ay = abs(dy);
        if (ax <= 2 && ay <= 2) return 1;
        if ((dx == 0 || dy == 0 || ax == ay) && ax <= 4 && ay <= 4) return 1;
    }
    return 0;
}

// 攻め側の手番の手。1 = 勝ち (*best)、-1 = 負け (手がない)、0 = ch に候補
static int gen_or(int *ch, int *nch, int *best) {
    Board *b = D.b;
    int att = D.att, def = D.def;
    *nch = 0;
    D.or_kind = 0;
    if (b->p4cnt[att][P4_FIVE]) { *best = find_p4(att, P4_FIVE); D.or_kind = 1; return 1; }
    int dfive = b->p4cnt[def][P4_FIVE];
    if (dfive >= 2) return -1;
    if (dfive == 1) {
        int q = find_p4(def, P4_FIVE);
        if (q < 0 || !dlegal(att, q)) return -1;
        ch[(*nch)++] = q;
        return 0;
    }
    if (!(g_dfpn_flags & 1) && tz_gate(b, att)) {
        TzZone z;
        int r = tz_probe_att(b, att, g_dfpn_vcf_nodes, &z);
        if (r && z.first >= 0) {
            *best = z.first;
            if (ZT) { D.or_kind = 3; D.or_zr = r; D.or_z.n = z.n; memcpy(D.or_z.cells, z.cells, sizeof(int) * z.n); D.or_z.first = z.first; }
            return 1;
        }
    }
    int tmp[MAX_MOVES], nt = near_cells(tmp), sc[MAX_CH], n = 0;
    int last = b->nmoves - 2 >= D.root_n ? b->moves[b->nmoves - 2] : -1;
    for (int i = 0; i < nt; i++) {
        int q = tmp[i], p4 = b->pat4[att][q];
        if (p4 == P4_FIVE) continue;
        if (p4 < P4_FLEX3) {
            // VCF の脅威: 三でなくても、打った後にパスしたら VCF がある手
            if (!g_dfpn_l2 || p4 < P4_BLOCK3 || !in_locality(q) || !dlegal(att, q)) continue;
            board_move_color(b, q, att);
            TzZone zq;
            int ok = tz_gate(b, att) && tz_probe_att(b, att, g_dfpn_vcf_nodes, &zq);
            board_undo(b);
            if (!ok) continue;
        }
        if (p4 == P4_FLEX4) {
            if (dlegal(att, q)) { *best = q; D.or_kind = 2; return 1; }
            continue;
        }
        int four = p4 >= P4_BLOCK4;
        if (!four && !in_locality(q)) continue;
        if (!dlegal(att, q)) continue;
        if (n >= MAX_CH) break;
        int s = p4 * 8 + (four ? 64 : 0);
        if (last >= 0) { int d = cheb(q, last); if (d < 6) s += 6 - d; }
        int j = n++;
        while (j > 0 && sc[j - 1] < s) { ch[j] = ch[j - 1]; sc[j] = sc[j - 1]; j--; }
        ch[j] = q;
        sc[j] = s;
    }
    *nch = n;
    return n ? 0 : -1;
}

static int zone_has(const TzZone *z, int q) {
    for (int i = 0; i < z->n; i++)
        if (z->cells[i] == q) return 1;
    return 0;
}

// 受け側の手番の手。1 = 攻め側の勝ち (受けがない)、-1 = 攻め側の負け (脅威でない・受け側の勝ち)、0 = ch に候補
static int gen_and(int *ch, int *nch) {
    Board *b = D.b;
    int att = D.att, def = D.def;
    *nch = 0;
    if (b->p4cnt[def][P4_FIVE]) return -1;
    int na = b->p4cnt[att][P4_FIVE];
    if (na >= 2) return 1;
    if (na == 1) {
        int f = find_p4(att, P4_FIVE);
        if (f < 0) return -1;
        if (!dlegal(def, f)) return 1;
        ch[(*nch)++] = f;
        return 0;
    }
    int tmp[MAX_MOVES], nt = near_cells(tmp);
    // 攻め側の脅威の圏: 達四の点ごとの手順の圏の共通部分、なければパスしたときの VCF の圏
    TzZone acc, z;
    acc.n = 0;
    int have = 0, all = 0;
    for (int i = 0; i < nt; i++) {
        int t = tmp[i];
        if (b->pat4[att][t] != P4_FLEX4 || !dlegal(att, t)) continue;
        if (tz_line_zone(b, att, &t, 1, &z) != 1) continue;
        if (!have) { acc = z; have = 1; continue; }
        int m = 0;
        for (int k = 0; k < acc.n; k++)
            if (zone_has(&z, acc.cells[k])) acc.cells[m++] = acc.cells[k];
        acc.n = m;
    }
    if (!have) {
        if (!tz_gate(b, att)) return -1;
        int r = tz_probe_att(b, att, g_dfpn_vcf_nodes, &z);
        if (!r) return -1;
        if (r == 2) all = 1;
        else acc = z;
    }
    D.zone = acc;
    D.zone_all = all;
    // 受け側が先に VCF で勝てるなら脅威は通らない
    if (!(g_dfpn_flags & 2) && tz_gate(b, def)) {
        TzZone zd;
        if (tz_probe_att(b, def, g_dfpn_vcf_nodes, &zd)) return -1;
    }
    static _Thread_local uint8_t seen[NCELL];
    int n = 0;
    if (all) {
        for (int i = 0; i < nt && n < MAX_CH; i++) ch[n++] = tmp[i];
        if (n >= MAX_CH) return -1;   // 受けが多すぎる (証明しない)
    } else {
        for (int i = 0; i < acc.n; i++) {
            int q = acc.cells[i];
            if (b->cell[q] == EMPTY && !seen[q]) { seen[q] = 1; ch[n++] = q; }
        }
    }
    for (int i = 0; i < nt; i++) {
        int q = tmp[i];
        if (!seen[q] && !all && b->pat4[def][q] >= P4_BLOCK4) {
            if (n >= MAX_CH) { for (int k = 0; k < n; k++) seen[ch[k]] = 0; return -1; }
            seen[q] = 1;
            ch[n++] = q;
        }
    }
    for (int k = 0; k < n; k++) seen[ch[k]] = 0;
    // 黒の禁じ手の受けは除く
    int m = 0;
    for (int k = 0; k < n; k++)
        if (dlegal(def, ch[k])) ch[m++] = ch[k];
    *nch = m;
    return m ? 0 : 1;
}

static inline uint32_t add_cap(uint32_t a, uint32_t b) {
    uint32_t s = a + b;
    return s >= INF || s < a ? INF : s;
}

// 局面の証明数・反証数を閾値まで調べる。is_or = 攻め側の手番
static void mid(int is_or, int ply, uint32_t thpn, uint32_t thdn, uint32_t *opn, uint32_t *odn) {
    Board *b = D.b;
    uint64_t key = dkey(b, is_or);
    DEntry *e = probe(key);
    if (e && (e->pn == 0 || e->dn == 0)) { *opn = e->pn; *odn = e->dn; return; }
    if (++D.nodes > D.max_nodes || (D.deadline > 0 && (D.nodes & 63) == 0 && now_ms() > D.deadline)) { D.aborted = 1; *opn = e ? e->pn : 1; *odn = e ? e->dn : 1; return; }
    int ch[MAX_CH], nch, best = -1;
    int st = is_or ? gen_or(ch, &nch, &best) : gen_and(ch, &nch);
    if (st == 0 && ply >= MAX_DEPTH) st = -1;
    if (st != 0) {
        // st は「攻め側から見て」1 = 勝ち、-1 = 負け
        uint32_t pn = st > 0 ? 0 : INF, dn = st > 0 ? INF : 0;
        store(key, pn, dn, best);
        if (st > 0 && ZT) {
            uint64_t w[ZW] = {0};
            if (is_or && D.or_kind == 1) zm_set(w, best);
            else if (is_or && D.or_kind == 2) {
                zm_set(w, best);
                board_move_color(b, best, D.att);
                zm_fives(w, D.att);
                board_undo(b);
            } else if (is_or && D.or_kind == 3 && D.or_zr == 1) {
                for (int i = 0; i < D.or_z.n; i++) zm_set(w, D.or_z.cells[i]);
                zm_set(w, D.or_z.first);
            } else if (!is_or && b->p4cnt[D.att][P4_FIVE] >= 2) zm_fives(w, D.att);
            else zm_all(w);   // 圏を絞れない VCF・受けが全部禁じ手など
            zm_base(w);
            zt_put(key, w);
        }
        *opn = pn;
        *odn = dn;
        return;
    }
    // 受け側の局面の脅威の圏 (証明できたときの圏に使う。子を読むと D.zone は上書きされる)
    int acc_n = 0, acc_all = 0, acc_cells[TZ_MAX_CELLS];
    if (!is_or && ZT) {
        acc_all = D.zone_all;
        acc_n = D.zone.n;
        memcpy(acc_cells, D.zone.cells, sizeof(int) * acc_n);
    }
    int color = is_or ? D.att : D.def;
    uint32_t cpn[MAX_CH], cdn[MAX_CH];
    for (int i = 0; i < nch; i++) {
        board_move_color(b, ch[i], color);
        DEntry *c = probe(dkey(b, !is_or));
        cpn[i] = c ? c->pn : 1;
        cdn[i] = c ? c->dn : 1;
        board_undo(b);
    }
    uint32_t pn = 0, dn = 0;
    for (;;) {
        // 子から証明数・反証数を計算
        int bi = 0;
        uint32_t b1 = INF + 1, b2 = INF + 1;
        if (is_or) {
            pn = INF; dn = 0;
            for (int i = 0; i < nch; i++) {
                dn = add_cap(dn, cdn[i]);
                if (cpn[i] < b1) { b2 = b1; b1 = cpn[i]; bi = i; }
                else if (cpn[i] < b2) b2 = cpn[i];
            }
            pn = b1 > INF ? INF : b1;
        } else {
            pn = 0; dn = INF;
            for (int i = 0; i < nch; i++) {
                pn = add_cap(pn, cpn[i]);
                if (cdn[i] < b1) { b2 = b1; b1 = cdn[i]; bi = i; }
                else if (cdn[i] < b2) b2 = cdn[i];
            }
            dn = b1 > INF ? INF : b1;
        }
        if (pn >= thpn || dn >= thdn || pn == 0 || dn == 0 || D.aborted) break;
        uint32_t s2 = b2 > INF ? INF : b2;
        uint32_t eps = s2 >= INF ? INF : s2 + s2 / 4 + 1;
        uint32_t tp, td;
        if (is_or) {
            tp = thpn < eps ? thpn : eps;
            td = thdn >= INF ? INF : thdn - dn + cdn[bi];
        } else {
            td = thdn < eps ? thdn : eps;
            tp = thpn >= INF ? INF : thpn - pn + cpn[bi];
        }
        board_move_color(b, ch[bi], color);
        mid(!is_or, ply + 1, tp, td, &cpn[bi], &cdn[bi]);
        board_undo(b);
    }
    if (is_or && pn == 0) {
        for (int i = 0; i < nch; i++)
            if (cpn[i] == 0) { best = ch[i]; break; }
    }
    store(key, pn, dn, best);
    if (pn == 0 && ZT) {
        // 証明の圏 = 局面の分 (受け側の四の点、受けの候補と脅威の圏) + 証明に使った子の圏の和
        uint64_t w[ZW] = {0};
        int ok = 1;
        uint64_t hb = b->hash ^ DSALT[D.att];
        if (is_or) {
            zm_set(w, best);
            ok = best >= 0 && child_zone(hb ^ board_zobrist(D.att, best) ^ ANDSALT, w);
        } else if (acc_all) ok = 0;
        else {
            for (int i = 0; i < acc_n; i++) zm_set(w, acc_cells[i]);
            for (int i = 0; i < nch && ok; i++) {
                zm_set(w, ch[i]);
                ok = child_zone(hb ^ board_zobrist(D.def, ch[i]), w);
            }
            if (ok && b->p4cnt[D.att][P4_FIVE]) zm_fives(w, D.att);
            if (ok && g_dfpn_zone_union && b->p4cnt[D.att][P4_FLEX4] >= 2)
                for (int y = 0; y < b->size; y++)
                    for (int x = 0; x < b->size; x++) {
                        int q = to_pos(x, y);
                        if (b->cell[q] != EMPTY || b->pat4[D.att][q] != P4_FLEX4 || !dlegal(D.att, q)) continue;
                        TzZone z;
                        int t = q;
                        if (tz_line_zone(b, D.att, &t, 1, &z) == 1)
                            for (int i = 0; i < z.n; i++) zm_set(w, z.cells[i]);
                        else ok = 0;
                    }
        }
        if (!ok) zm_all(w);
        zm_base(w);
        zt_put(key, w);
    }
    *opn = pn;
    *odn = dn;
}

// 対局の前に (探索するスレッドで) 表を確保して全ページに触れておく
void dfpn_warm(void) {
    if (g_dfpn_zt && !ZT) {
        ZT = calloc(1u << ZT_BITS, sizeof(ZEnt));
        if (ZT) memset(ZT, 0, sizeof(ZEnt) * (1u << ZT_BITS));
    }
    if (D.tt) return;
    D.tt = calloc(DTT_SIZE, sizeof(DEntry));
    if (D.tt) memset(D.tt, 0, sizeof(DEntry) * DTT_SIZE);
    D.shadow = -1;
}

static int dfpn_run(Board *b, int attacker, long max_nodes, int max_ms, DfpnResult *res, int resume);

int dfpn_vct(Board *b, int attacker, long max_nodes, int max_ms, DfpnResult *res) {
    return dfpn_run(b, attacker, max_nodes, max_ms, res, 0);
}

// 直前の dfpn_vct と同じ盤・同じ攻め側なら、置換表の世代を変えずに続きから調べる (証明数・反証数の途中の値を使い回す)。
// 違えば dfpn_vct と同じ
int dfpn_vct_resume(Board *b, int attacker, long max_nodes, int max_ms, DfpnResult *res) {
    int same = D.tt && D.b == b && D.att == attacker && D.root_hash == b->hash && D.root_n == b->nmoves;
    return dfpn_run(b, attacker, max_nodes, max_ms, res, same);
}

// shadow の点に受け側の石がある盤で、その石を除いた局面の証明 (圏の外のもの) を使い回して調べる
int dfpn_vct_shadow(Board *b, int attacker, int shadow, long max_nodes, int max_ms, DfpnResult *res) {
    D.shadow = shadow;
    D.shadow_z = board_zobrist(opp_color(attacker), shadow);
    D.shadow_hits = 0;
    int w = dfpn_run(b, attacker, max_nodes, max_ms, res, 0);
    dfpn_zt_stat[2] += D.shadow_hits;
    D.shadow = -1;
    return w;
}

static int dfpn_run(Board *b, int attacker, long max_nodes, int max_ms, DfpnResult *res, int resume) {
    if (!D.tt) {
        D.tt = calloc(DTT_SIZE, sizeof(DEntry));
        if (!D.tt) return 0;
        D.shadow = -1;
    }
    if (g_dfpn_zt && !ZT) ZT = calloc(1u << ZT_BITS, sizeof(ZEnt));
    if (!g_dfpn_zt) ZT = NULL;   // (表は捨てない: 調べ直し用に 0 にしたときは使わないだけ)
    if (!resume) D.gen++;
    D.root_hash = b->hash;
    D.b = b;
    D.att = attacker;
    D.def = opp_color(attacker);
    D.forb = rule_has_forbidden(&b->rule);
    D.root_n = b->nmoves;
    D.nodes = 0;
    D.max_nodes = max_nodes;
    D.deadline = max_ms > 0 ? now_ms() + max_ms : 0;
    D.aborted = 0;
    uint32_t pn = 1, dn = 1;
    while (pn && dn && !D.aborted) mid(1, 0, INF, INF, &pn, &dn);
    int win = pn == 0, move = -1;
    if (win) {
        DEntry *e = probe(dkey(b, 1));
        move = e ? e->best : -1;
    }
    if (res) {
        res->win = win;
        res->move = move;
        res->nodes = D.nodes;
        res->aborted = D.aborted;
    }
    return win;
}

// ---- 証明の圏 (dfpn_vct_zone) ----
// 証明木をたどって、攻め側の手・受けの候補・各 AND 局面の脅威の圏・末端の VCF の圏を集める。
// この外に受け側の石が 1 つ増えても、証明木の各局面の受けの候補は (受け側の新しい四を除いて) 変わらない
static _Thread_local struct {
    uint8_t mark[NCELL];
    int *out, n, max;
    long steps;
    int fail;
    uint64_t seen[4096];
} Z;

static void zadd(int q) {
    if (q < 0 || Z.mark[q]) return;
    if (Z.n >= Z.max) { Z.fail = 1; return; }
    Z.mark[q] = 1;
    Z.out[Z.n++] = q;
}

static int zseen(uint64_t key) {
    uint64_t *s = &Z.seen[key & 4095];
    if (*s == key) return 1;
    *s = key;
    return 0;
}

static void zwalk(int is_or, int ply) {
    Board *b = D.b;
    if (Z.fail || ++Z.steps > 20000 || ply > MAX_DEPTH) { Z.fail = 1; return; }
    if (zseen(dkey(b, is_or))) return;
    int att = D.att, def = D.def;
    // 受け側の石が 1 つ増えて受け側の四の点が新しくできると、証明木の受け (受け側の四) が変わり得る: その点も圏に入れる
    if (g_dfpn_zone_def)
        for (int q = 0; q < NCELL; q++)
            if (b->cell[q] == EMPTY && b->pat4[def][q] >= g_dfpn_zone_def && b->pat4[def][q] != P4_FLEX2_2X) zadd(q);
    if (is_or) {
        if (b->p4cnt[att][P4_FIVE]) { zadd(find_p4(att, P4_FIVE)); return; }
        if (b->p4cnt[def][P4_FIVE] == 0 && tz_gate(b, att)) {
            TzZone z;
            int r = tz_probe_att(b, att, 20000, &z);
            if (r == 2) { Z.fail = 1; return; }
            if (r == 1) {
                for (int i = 0; i < z.n; i++) zadd(z.cells[i]);
                zadd(z.first);
                return;
            }
        }
        DEntry *e = probe(dkey(b, 1));
        int m = e && e->pn == 0 ? e->best : -1;
        if (b->p4cnt[def][P4_FIVE] == 1) m = find_p4(def, P4_FIVE);
        if (m < 0 || b->cell[m] != EMPTY) { Z.fail = 1; return; }
        zadd(m);
        board_move_color(b, m, att);
        zwalk(0, ply + 1);
        board_undo(b);
        return;
    }
    int ch[MAX_CH], nch;
    D.zone.n = 0;
    D.zone_all = 0;
    int na = b->p4cnt[att][P4_FIVE];
    int st = gen_and(ch, &nch);
    if (st < 0 || D.zone_all) { Z.fail = 1; return; }
    if (na) {
        for (int q = 0; q < NCELL; q++)
            if (b->cell[q] == EMPTY && b->pat4[att][q] == P4_FIVE) zadd(q);
    } else {
        for (int i = 0; i < D.zone.n; i++) zadd(D.zone.cells[i]);
        // 達四の点が複数あるとき、受けの候補は各点の圏の共通部分だが、圏としては和が要る
        // (片方の圏にだけ受け側の石が増えると、その点の圏が変わって新しい受けが生まれ得る)
        if (g_dfpn_zone_union && b->p4cnt[att][P4_FLEX4] >= 2)
            for (int q = 0; q < NCELL; q++) {
                if (b->cell[q] != EMPTY || b->pat4[att][q] != P4_FLEX4 || !dlegal(att, q)) continue;
                TzZone z;
                int t = q;
                if (tz_line_zone(b, att, &t, 1, &z) == 1)
                    for (int i = 0; i < z.n; i++) zadd(z.cells[i]);
            }
    }
    if (st > 0) return;
    int save[MAX_CH];
    memcpy(save, ch, sizeof(int) * nch);
    for (int i = 0; i < nch && !Z.fail; i++) {
        zadd(save[i]);
        board_move_color(b, save[i], def);
        zwalk(1, ply + 1);
        board_undo(b);
    }
}

int dfpn_vct_zone(Board *b, int attacker, int *cells, int max) {
    // dfpn_vct の直後 (同じ盤・同じ攻め側) に呼ぶ。D の状態はそのまま使う
    if (D.b != b || D.att != attacker) return -1;
    memset(Z.mark, 0, sizeof Z.mark);
    memset(Z.seen, 0, sizeof Z.seen);
    Z.out = cells;
    Z.n = 0;
    Z.max = max;
    Z.steps = 0;
    Z.fail = 0;
    long nodes = D.nodes;
    D.root_n = b->nmoves;
    zwalk(1, 0);
    D.nodes = nodes;
    return Z.fail ? -1 : Z.n;
}
