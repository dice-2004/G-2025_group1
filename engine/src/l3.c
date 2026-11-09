// λ3 の証明数探索 (説明は l3.h)
#include "l3.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "dfpn.h"
#include "tz.h"
#include "vcf.h"

long g_l3_vct_nodes = 20000;    // AND の局面の圏を作る dfpn_vct
long g_l3_quiet_nodes = 2000;   // 手の確認 (tss_solve の VCT、浅い反復深化)
int g_l3_vct_depth = 6;
int g_l3_def_min = P4_FLEX3;   // 受け側の反撃の手として候補に入れる pat4 の下限
int g_l3_or_mode = 0;      // OR の局面の VCT の確認: 0 = tss と dfpn、1 = tss だけ、2 = dfpn だけ
long g_l3_or_nodes = 2000;
long g_l3_stat[6];
double g_l3_ms[4];   // 調査用: OR の VCT, 静かな手の確認, AND の dfpn+圏, AND の quick   // 調査用: OR 数, OR の子の合計, AND 数, AND の子の合計, AND の圏の失敗, AND の dfpn 失敗
int g_l3_quiet_min = P4_FLEX2_2X;

#define INF 100000000u
#define LTT_BITS 16
#define LTT_SIZE (1u << LTT_BITS)
#define GC_BITS 12
#define GC_SIZE (1u << GC_BITS)
#define MAX_CH 64
#define MAX_DEPTH 60
#define ZMAX 400

typedef struct {
    uint64_t key;
    uint32_t pn, dn;
    int16_t best;
    uint16_t gen;
} LEntry;

// 候補手の作り直しは dfpn_vct を何度も呼ぶので重い。局面ごとに覚える
typedef struct {
    uint64_t key;
    int16_t st, n;
    int16_t ch[MAX_CH];
} GEntry;

static _Thread_local struct {
    LEntry *tt;
    GEntry *gc;
    uint16_t gen;
    Board *b;
    int att, def, forb, root_n;
    long nodes, max_nodes, vct_calls;
    double deadline;
    int aborted;
} L;

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec * 1e-6;
}

static const uint64_t LSALT[3] = {0, 0x9e3779b97f4a7c15ULL, 0xc2b2ae3d27d4eb4fULL};
#define LANDSALT 0x165667b19e3779f9ULL
static inline uint64_t lkey(const Board *b, int is_or) { return b->hash ^ LSALT[L.att] ^ (is_or ? 0 : LANDSALT); }

static inline int llegal(int color, int q) {
    return !(L.forb && color == BLACK && board_is_forbidden(L.b, q));
}

static LEntry *probe(uint64_t key) {
    LEntry *e = &L.tt[key & (LTT_SIZE - 1)];
    if (e->key != key) return NULL;
    if (e->gen != L.gen && e->pn != 0) return NULL;
    return e;
}

static void store(uint64_t key, uint32_t pn, uint32_t dn, int best) {
    LEntry *e = &L.tt[key & (LTT_SIZE - 1)];
    if (e->key == key && e->pn == 0 && pn != 0) return;
    e->key = key;
    e->pn = pn;
    e->dn = dn;
    e->best = (int16_t)best;
    e->gen = L.gen;
}

static int find_p4(int color, int p4) {
    const Board *b = L.b;
    for (int q = 0; q < NCELL; q++)
        if (b->cell[q] == EMPTY && b->pat4[color][q] == p4) return q;
    return -1;
}

static int vct(int att, long nodes) {
    if (L.deadline > 0 && now_ms() > L.deadline) { L.aborted = 1; return 0; }
    L.vct_calls++;
    DfpnResult r;
    return dfpn_vct(L.b, att, nodes, 0, &r);
}

// 速い確認: 短い VCT (tss_solve の反復深化)。見つからなくても長い VCT はあり得る
static int vct_quick(int att, long nodes) {
    if (L.deadline > 0 && now_ms() > L.deadline) { L.aborted = 1; return 0; }
    L.vct_calls++;
    return tss_solve(L.b, att, TSS_VCT, g_l3_vct_depth, nodes, NULL);
}

// 静かな手を探す範囲: 探索中に打った攻め側の直近 2 手から 3 マス以内 (根では全体)
static int near_last(int q) {
    const Board *b = L.b;
    if (b->nmoves - L.root_n < 2) return 1;
    for (int k = 0, i = b->nmoves - 2; k < 2 && i >= L.root_n; k++, i -= 2) {
        int m = b->moves[i];
        int dx = abs(pos_x(q) - pos_x(m)), dy = abs(pos_y(q) - pos_y(m));
        if (dx <= 3 && dy <= 3) return 1;
    }
    return 0;
}

// 攻め側の手番: 1 = 勝ち (VCT がある)、-1 = 手がない、0 = 候補 (打って受け側がパスしたら VCT がある手)
static int gen_or(int *ch, int *nch, int *best) {
    Board *b = L.b;
    int att = L.att, def = L.def;
    *nch = 0;
    if (b->p4cnt[att][P4_FIVE]) { *best = find_p4(att, P4_FIVE); return 1; }
    int dfive = b->p4cnt[def][P4_FIVE];
    if (dfive >= 2) return -1;
    if (dfive == 1) {
        int q = find_p4(def, P4_FIVE);
        if (q < 0 || !llegal(att, q)) return -1;
        ch[(*nch)++] = q;
        return 0;
    }
    double t0 = now_ms();
    if ((g_l3_or_mode != 2 && vct_quick(att, g_l3_or_nodes)) || (g_l3_or_mode != 1 && vct(att, g_l3_or_nodes * 2))) { g_l3_ms[0] += now_ms() - t0; return 1; }
    double t1 = now_ms(); g_l3_ms[0] += t1 - t0;
    int sc[MAX_CH], n = 0;
    for (int q = 0; q < NCELL && !L.aborted; q++) {
        if (b->cell[q] != EMPTY || !b->near[q]) continue;
        int p4 = b->pat4[att][q];
        if (p4 < g_l3_quiet_min || p4 == P4_FIVE) continue;
        if (p4 < P4_BLOCK4 && !near_last(q)) continue;
        if (!llegal(att, q)) continue;
        board_move_color(b, q, att);
        int ok = vct_quick(att, g_l3_quiet_nodes);
        board_undo(b);
        if (!ok) continue;
        if (n >= MAX_CH) break;
        int s = p4;
        int j = n++;
        while (j > 0 && sc[j - 1] < s) { ch[j] = ch[j - 1]; sc[j] = sc[j - 1]; j--; }
        ch[j] = q;
        sc[j] = s;
    }
    g_l3_ms[1] += now_ms() - t1;
    *nch = n;
    g_l3_stat[0]++; g_l3_stat[1] += n;
    return n ? 0 : -1;
}

// 受け側の手番: 1 = 攻め側の勝ち、-1 = 脅威でない・受け側が先に勝つ、0 = 候補
// 候補 = 「パスしたら攻め側が VCT で勝つ」証明の圏の空点 + 受け側の三・四
static int gen_and(int *ch, int *nch) {
    Board *b = L.b;
    int att = L.att, def = L.def;
    *nch = 0;
    if (b->p4cnt[def][P4_FIVE]) return -1;
    int na = b->p4cnt[att][P4_FIVE];
    if (na >= 2) return 1;
    if (na == 1) {
        int f = find_p4(att, P4_FIVE);
        if (f < 0) return -1;
        if (!llegal(def, f)) return 1;
        ch[(*nch)++] = f;
        return 0;
    }
    if (tz_gate(b, def)) {
        TzZone zd;
        if (tz_probe_att(b, def, 200, &zd)) return -1;
    }
    static _Thread_local int zc[ZMAX];
    static _Thread_local uint8_t seen[NCELL];
    int nz = -1, n = 0;
    double t0 = now_ms();
    if (vct(att, g_l3_vct_nodes)) { nz = dfpn_vct_zone(b, att, zc, ZMAX); if (nz < 0) g_l3_stat[4]++; }
    else if (g_l3_stat[5]++, 0) ;
    else if (!vct_quick(att, g_l3_quiet_nodes * 4)) { g_l3_ms[3] += now_ms() - t0; return -1; }
    g_l3_ms[2] += now_ms() - t0;
    if (nz < 0) {
        nz = 0;
        for (int q = 0; q < NCELL && nz < ZMAX; q++)
            if (b->cell[q] == EMPTY && b->near[q]) zc[nz++] = q;
    }
    for (int i = 0; i < nz; i++) {
        int q = zc[i];
        if (b->cell[q] != EMPTY || seen[q]) continue;
        if (n >= MAX_CH) break;
        seen[q] = 1;
        ch[n++] = q;
    }
    for (int q = 0; q < NCELL && n < MAX_CH; q++)
        if (b->cell[q] == EMPTY && !seen[q] && b->near[q] && b->pat4[def][q] >= g_l3_def_min) { seen[q] = 1; ch[n++] = q; }
    int over = n >= MAX_CH;
    for (int k = 0; k < n; k++) seen[ch[k]] = 0;
    if (over) return -1;   // 受けが多すぎる (証明しない)
    int m = 0;
    for (int k = 0; k < n; k++)
        if (llegal(def, ch[k])) ch[m++] = ch[k];
    *nch = m;
    g_l3_stat[2]++; g_l3_stat[3] += m;
    return m ? 0 : 1;
}

static int gen_cached(int is_or, int *ch, int *nch, int *best) {
    uint64_t key = lkey(L.b, is_or);
    GEntry *g = &L.gc[key & (GC_SIZE - 1)];
    if (g->key == key) {
        *nch = g->n;
        for (int i = 0; i < g->n; i++) ch[i] = g->ch[i];
        if (g->st > 0 && is_or) *best = g->ch[0];
        return g->st;
    }
    *best = -1;
    int st = is_or ? gen_or(ch, nch, best) : gen_and(ch, nch);
    if (L.aborted) return st;
    g->key = key;
    g->st = (int16_t)st;
    g->n = (int16_t)*nch;
    for (int i = 0; i < *nch; i++) g->ch[i] = (int16_t)ch[i];
    if (st > 0 && is_or) { g->n = 0; g->ch[0] = (int16_t)*best; }
    return st;
}

static inline uint32_t add_cap(uint32_t a, uint32_t b) {
    uint32_t s = a + b;
    return s >= INF || s < a ? INF : s;
}

static void mid(int is_or, int ply, uint32_t thpn, uint32_t thdn, uint32_t *opn, uint32_t *odn) {
    Board *b = L.b;
    uint64_t key = lkey(b, is_or);
    LEntry *e = probe(key);
    if (e && (e->pn == 0 || e->dn == 0)) { *opn = e->pn; *odn = e->dn; return; }
    if (++L.nodes > L.max_nodes || (L.deadline > 0 && now_ms() > L.deadline)) { L.aborted = 1; *opn = e ? e->pn : 1; *odn = e ? e->dn : 1; return; }
    int ch[MAX_CH], nch, best = -1;
    int st = gen_cached(is_or, ch, &nch, &best);
    if (L.aborted) { *opn = e ? e->pn : 1; *odn = e ? e->dn : 1; return; }
    if (st == 0 && ply >= MAX_DEPTH) st = -1;
    if (st != 0) {
        uint32_t pn = st > 0 ? 0 : INF, dn = st > 0 ? INF : 0;
        store(key, pn, dn, best);
        *opn = pn;
        *odn = dn;
        return;
    }
    int color = is_or ? L.att : L.def;
    uint32_t cpn[MAX_CH], cdn[MAX_CH];
    for (int i = 0; i < nch; i++) {
        board_move_color(b, ch[i], color);
        LEntry *c = probe(lkey(b, !is_or));
        cpn[i] = c ? c->pn : 1;
        cdn[i] = c ? c->dn : 1;
        board_undo(b);
    }
    uint32_t pn = 0, dn = 0;
    for (;;) {
        int bi = 0;
        uint32_t b1 = INF + 1, b2 = INF + 1;
        if (is_or) {
            dn = 0;
            for (int i = 0; i < nch; i++) {
                dn = add_cap(dn, cdn[i]);
                if (cpn[i] < b1) { b2 = b1; b1 = cpn[i]; bi = i; }
                else if (cpn[i] < b2) b2 = cpn[i];
            }
            pn = b1 > INF ? INF : b1;
        } else {
            pn = 0;
            for (int i = 0; i < nch; i++) {
                pn = add_cap(pn, cpn[i]);
                if (cdn[i] < b1) { b2 = b1; b1 = cdn[i]; bi = i; }
                else if (cdn[i] < b2) b2 = cdn[i];
            }
            dn = b1 > INF ? INF : b1;
        }
        if (pn >= thpn || dn >= thdn || pn == 0 || dn == 0 || L.aborted) break;
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
    *opn = pn;
    *odn = dn;
}

int l3_win(Board *b, int attacker, long max_nodes, int max_ms, L3Result *res) {
    if (!L.tt) {
        L.tt = calloc(LTT_SIZE, sizeof(LEntry));
        L.gc = calloc(GC_SIZE, sizeof(GEntry));
        if (!L.tt || !L.gc) return 0;
    }
    L.gen++;
    memset(L.gc, 0, GC_SIZE * sizeof(GEntry));   // 候補は予算や打ち切りで変わるので呼び出しごとに作り直す
    L.b = b;
    L.att = attacker;
    L.def = opp_color(attacker);
    L.forb = rule_has_forbidden(&b->rule);
    L.root_n = b->nmoves;
    L.nodes = 0;
    L.vct_calls = 0;
    L.max_nodes = max_nodes;
    L.deadline = max_ms > 0 ? now_ms() + max_ms : 0;
    L.aborted = 0;
    // まず普通の VCT を時間の半分まで (長い VCT は λ3 の中の短い確認では見えない)
    DfpnResult dr;
    if (dfpn_vct(b, attacker, max_nodes, max_ms > 0 ? max_ms / 2 : 0, &dr)) {
        if (res) { res->win = 1; res->move = dr.move; res->nodes = 0; res->vct_calls = 1; res->aborted = 0; }
        return 1;
    }
    uint32_t pn = 1, dn = 1;
    while (pn && dn && !L.aborted) mid(1, 0, INF, INF, &pn, &dn);
    int win = pn == 0, move = -1;
    if (win) {
        LEntry *e = probe(lkey(b, 1));
        move = e ? e->best : -1;
    }
    if (res) {
        res->win = win;
        res->move = move;
        res->nodes = L.nodes;
        res->vct_calls = L.vct_calls;
        res->aborted = L.aborted;
    }
    return win;
}
