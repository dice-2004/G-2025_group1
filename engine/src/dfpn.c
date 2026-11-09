// 証明数探索 (df-pn) による VCT (説明は dfpn.h)
#include "dfpn.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tz.h"

long g_dfpn_vcf_nodes = 100;   // 長い VCF はルートの vcf_solve が別に調べる。小さいほうが同じ時間で詰みを多く見つける
int g_dfpn_zone_k = 2;

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
} D;

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec * 1e-6;
}

static const uint64_t DSALT[3] = {0, 0x243f6a8885a308d3ULL, 0x13198a2e03707344ULL};

static inline int dlegal(int color, int q) {
    return !(D.forb && color == BLACK && board_is_forbidden(D.b, q));
}

static DEntry *probe(uint64_t key) {
    DEntry *e = &D.tt[key & (DTT_SIZE - 1)];
    if (e->key != key) return NULL;
    if (e->gen != D.gen && e->pn != 0) return NULL;   // 前の呼び出しの証明 (経路によらず正しい) だけ使う
    return e;
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
    if (b->p4cnt[att][P4_FIVE]) { *best = find_p4(att, P4_FIVE); return 1; }
    int dfive = b->p4cnt[def][P4_FIVE];
    if (dfive >= 2) return -1;
    if (dfive == 1) {
        int q = find_p4(def, P4_FIVE);
        if (q < 0 || !dlegal(att, q)) return -1;
        ch[(*nch)++] = q;
        return 0;
    }
    if (tz_gate(b, att)) {
        TzZone z;
        if (tz_probe_att(b, att, g_dfpn_vcf_nodes, &z) && z.first >= 0) { *best = z.first; return 1; }
    }
    int tmp[MAX_MOVES], nt = near_cells(tmp), sc[MAX_CH], n = 0;
    int last = b->nmoves - 2 >= D.root_n ? b->moves[b->nmoves - 2] : -1;
    for (int i = 0; i < nt; i++) {
        int q = tmp[i], p4 = b->pat4[att][q];
        if (p4 < P4_FLEX3 || p4 == P4_FIVE) continue;
        if (p4 == P4_FLEX4) {
            if (dlegal(att, q)) { *best = q; return 1; }
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
    // 受け側が先に VCF で勝てるなら脅威は通らない
    if (tz_gate(b, def)) {
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
    uint64_t key = b->hash ^ DSALT[D.att];
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
        *opn = pn;
        *odn = dn;
        return;
    }
    int color = is_or ? D.att : D.def;
    uint32_t cpn[MAX_CH], cdn[MAX_CH];
    for (int i = 0; i < nch; i++) {
        board_move_color(b, ch[i], color);
        DEntry *c = probe(b->hash ^ DSALT[D.att]);
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
    *opn = pn;
    *odn = dn;
}

int dfpn_vct(Board *b, int attacker, long max_nodes, int max_ms, DfpnResult *res) {
    if (!D.tt) {
        D.tt = calloc(DTT_SIZE, sizeof(DEntry));
        if (!D.tt) return 0;
    }
    D.gen++;
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
        DEntry *e = probe(b->hash ^ DSALT[attacker]);
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
