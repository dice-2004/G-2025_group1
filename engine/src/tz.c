// 脅威圏探索 (説明は tz.h)
#include "tz.h"

#include <stdlib.h>
#include <string.h>

#include "vcf.h"

#define TZ_LINE_MAX 48
#define CACHE_BITS 15
#define CACHE_SIZE (1u << CACHE_BITS)
#define RING 4

enum { C_EMPTY = 0, C_NONE = 1, C_HAS = 2 };

typedef struct {
    uint64_t key;
    uint8_t result;
    uint8_t n;
    int16_t line[TZ_LINE_MAX];
} CacheEntry;

typedef struct {
    CacheEntry *cache;
    int16_t ring[3][RING][TZ_LINE_MAX];   // [攻め側の色]
    int ring_n[3][RING];
    int ring_head[3];
    uint32_t stamp[NCELL];
    uint32_t gen;
    int epoch;
    TzStats st;
} TzState;

static _Thread_local TzState *S;
static int tz_epoch;   // tz_clear のたびに増やす (各スレッドの表は次に使う時に消す)

static const uint64_t SALT[3] = {0, 0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL};

void tz_clear(void) { __atomic_add_fetch(&tz_epoch, 1, __ATOMIC_RELAXED); }

static int ensure(void) {
    if (S) {
        int ep = __atomic_load_n(&tz_epoch, __ATOMIC_RELAXED);
        if (S->epoch != ep) {
            memset(S->cache, 0, sizeof(CacheEntry) * CACHE_SIZE);
            memset(S->ring_n, 0, sizeof S->ring_n);
            S->epoch = ep;
        }
        return 1;
    }
    S = calloc(1, sizeof(TzState));
    if (!S) return 0;
    S->cache = calloc(CACHE_SIZE, sizeof(CacheEntry));
    if (!S->cache) { free(S); S = NULL; return 0; }
    S->epoch = __atomic_load_n(&tz_epoch, __ATOMIC_RELAXED);
    return 1;
}

TzStats tz_stats(void) {
    TzStats z = {0};
    return S ? S->st : z;
}

void tz_reset_stats(void) {
    if (S) memset(&S->st, 0, sizeof S->st);
}

static inline void add(const Board *b, TzZone *z, int q) {
    if (q < 0 || q >= NCELL || b->cell[q] != EMPTY || S->stamp[q] == S->gen || z->n >= TZ_MAX_CELLS) return;
    S->stamp[q] = S->gen;
    z->cells[z->n++] = q;
}

// m を通る 4 方向 ±4 マスの color の五の点を z に足す
static void add_fives_near(const Board *b, TzZone *z, int color, int m) {
    for (int d = 0; d < 4; d++)
        for (int k = -4; k <= 4; k++) {
            int q = m + k * DIR_OFFSET[d];
            if (k && q >= 0 && q < NCELL && b->cell[q] == EMPTY && b->pat4[color][q] == P4_FIVE) add(b, z, q);
        }
}

static int five_near(const Board *b, int color, int m) {
    for (int d = 0; d < 4; d++)
        for (int k = -4; k <= 4; k++) {
            int q = m + k * DIR_OFFSET[d];
            if (k && q >= 0 && q < NCELL && b->cell[q] == EMPTY && b->pat4[color][q] == P4_FIVE) return q;
        }
    return -1;
}

// att の VCF 手順 line (攻め・受けが交互) を今の盤で再生して確かめ、圏を z に作る。
// 戻り値 1 = 手順は有効 (圏あり)、2 = 有効だが圏を絞れない、0 = この盤では成り立たない
static int replay(Board *b, int att, const int16_t *line, int n, TzZone *z) {
    int def = opp_color(att);
    int forb = rule_has_forbidden(&b->rule);
    int restrict_a = forb && att == BLACK, restrict_d = forb && def == BLACK;
    int played = 0, ok = 0, i = 0, last = -1;
    S->gen++;
    z->n = 0;
    z->first = n > 0 ? line[0] : -1;
    for (;;) {
        if (b->p4cnt[att][P4_FIVE]) {
            // 攻め側の五の点 (直前の攻めの手の周りにしかできない)
            if (last >= 0) { add_fives_near(b, z, att, last); ok = 1; }
            break;
        }
        if (i >= n) break;
        int a = line[i++];
        if (a < 0 || a >= NCELL || b->cell[a] != EMPTY) break;
        int dfive = b->p4cnt[def][P4_FIVE];
        if (dfive >= 2 || (dfive == 1 && b->pat4[def][a] != P4_FIVE)) break;   // 受け側の四を止めていない
        if (restrict_a && board_is_forbidden(b, a)) break;
        add(b, z, a);
        board_move_color(b, a, att);
        played++;
        last = a;
        if (b->p4cnt[def][P4_FIVE]) break;
        int nf = b->p4cnt[att][P4_FIVE];
        if (nf == 0) break;                                 // 四になっていない
        if (nf >= 2) { add_fives_near(b, z, att, a); ok = 1; break; }
        int q = five_near(b, att, a);
        if (q < 0) break;
        if (restrict_d && board_is_forbidden(b, q)) { ok = 2; break; }   // 止める点が黒の禁じ手
        add(b, z, q);
        // 受け側が止めた石に石を足して四にできる方向 (そこに置くと三以上) は ±4 マスを圏に入れる
        for (int d = 0; d < 4; d++) {
            if (b->pat[def][q][d] < P_B3) continue;
            for (int k = -4; k <= 4; k++) add(b, z, q + k * DIR_OFFSET[d]);
        }
        if (i < n) i++;   // 手順の受けの手 (止める点しかない)
        board_move_color(b, q, def);
        played++;
    }
    z->plies = played + 1;
    while (played--) board_undo(b);
    return ok;
}

int tz_line_zone(Board *b, int att, const int *line, int n, TzZone *z) {
    if (!ensure()) return 0;
    int16_t l[TZ_LINE_MAX];
    if (n > TZ_LINE_MAX) return 0;
    for (int i = 0; i < n; i++) l[i] = (int16_t)line[i];
    return replay(b, att, l, n, z);
}

static void remember(CacheEntry *e, uint64_t key, int result, const int16_t *line, int n) {
    e->key = key;
    e->result = (uint8_t)result;
    e->n = (uint8_t)n;
    if (n) memcpy(e->line, line, sizeof(int16_t) * n);
}

int tz_probe(Board *b, long max_nodes, TzZone *z) {
    return tz_probe_att(b, opp_color(board_side(b)), max_nodes, z);
}

int tz_probe_att(Board *b, int att, long max_nodes, TzZone *z) {
    if (!ensure()) return 0;
    S->st.probes++;
    uint64_t key = b->hash ^ SALT[att];
    CacheEntry *e = &S->cache[key & (CACHE_SIZE - 1)];
    if (e->key == key && e->result != C_EMPTY) {
        S->st.cache_hits++;
        if (e->result == C_NONE) return 0;
        int r = replay(b, att, e->line, e->n, z);
        return r ? r : 2;
    }
    // 直近に見つけた手順がこの局面でも通るか
    for (int k = 0; k < RING; k++) {
        int idx = (S->ring_head[att] - 1 - k + RING) % RING;
        if (!S->ring_n[att][idx]) continue;
        int r = replay(b, att, S->ring[att][idx], S->ring_n[att][idx], z);
        if (r) {
            S->st.replay_hits++;
            remember(e, key, C_HAS, S->ring[att][idx], S->ring_n[att][idx]);
            return r;
        }
    }
    S->st.solves++;
    TssResult tr;
    tss_solve(b, att, TSS_VCF, 30, max_nodes, &tr);
    if (!tr.win) {
        remember(e, key, C_NONE, NULL, 0);
        return 0;
    }
    S->st.found++;
    int n = tr.nline < TZ_LINE_MAX ? tr.nline : TZ_LINE_MAX;
    int16_t line[TZ_LINE_MAX];
    for (int i = 0; i < n; i++) line[i] = (int16_t)tr.line[i];
    remember(e, key, C_HAS, line, n);
    memcpy(S->ring[att][S->ring_head[att]], line, sizeof(int16_t) * n);
    S->ring_n[att][S->ring_head[att]] = n;
    S->ring_head[att] = (S->ring_head[att] + 1) % RING;
    int r = replay(b, att, line, n, z);
    return r ? r : 2;
}

// ---------------------------------------------------------------- λ2 探索

#define L2_TT_BITS 18
#define L2_TT_SIZE (1u << L2_TT_BITS)
#define L2_MAX_CAND 48

typedef struct {
    uint64_t key;
    int16_t depth;      // L2_FAIL: この深さまで勝ちなし
    int16_t best;
    uint8_t result;
} L2Entry;

enum { L2_NONE = 0, L2_WIN = 1, L2_FAIL = 2 };

static _Thread_local struct {
    L2Entry *tt;
    int epoch;
    long nodes, max_nodes, vcf_nodes;
    int aborted;
    int att, def;
    int forb;
} L;

static const uint64_t L2_SALT[3] = {0, 0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL};

static inline int l2_legal(Board *b, int color, int q) {
    return !(L.forb && color == BLACK && board_is_forbidden(b, q));
}

// 石の範囲 ±2 の空点 (b->near > 0) を out に
static int near_cells(const Board *b, int *out) {
    int n = 0;
    for (int y = 0; y < b->size; y++) {
        int q = to_pos(0, y);
        for (int x = 0; x < b->size; x++, q++)
            if (b->cell[q] == EMPTY && b->near[q]) out[n++] = q;
    }
    return n;
}

static int l2_or(Board *b, int depth, int *best);

// 攻め側が q に打った後 (受け側の手番)。攻め側の勝ちを証明できれば 1
static int l2_and(Board *b, int depth, const TzZone *z, int zone_all) {
    int att = L.att, def = L.def;
    int cand[MAX_MOVES], nc = 0;
    static _Thread_local uint8_t seen[NCELL];
    if (zone_all) {
        nc = near_cells(b, cand);
    } else {
        for (int i = 0; i < z->n; i++) cand[nc++] = z->cells[i];
    }
    for (int i = 0; i < nc; i++) seen[cand[i]] = 1;
    // 受け側の四 (手番を取り返す手) も試す
    int tmp[MAX_MOVES], nt = near_cells(b, tmp);
    for (int i = 0; i < nt; i++)
        if (!seen[tmp[i]] && b->pat4[def][tmp[i]] >= P4_BLOCK4) { seen[tmp[i]] = 1; cand[nc++] = tmp[i]; }
    for (int i = 0; i < nc; i++) seen[cand[i]] = 0;
    (void)att;
    for (int i = 0; i < nc; i++) {
        int d = cand[i];
        if (b->cell[d] != EMPTY || !l2_legal(b, def, d)) continue;
        board_move_color(b, d, def);
        int bm;
        int w = l2_or(b, depth - 1, &bm);
        board_undo(b);
        if (!w) return 0;
        if (L.aborted) return 0;
    }
    return 1;
}

// 攻め側の手番。勝ちを証明できれば 1 (*best に手)
static int l2_or(Board *b, int depth, int *best) {
    int att = L.att, def = L.def;
    if (++L.nodes > L.max_nodes) { L.aborted = 1; return 0; }
    *best = -1;
    if (b->p4cnt[att][P4_FIVE]) {
        for (int y = 0; y < b->size; y++)
            for (int x = 0; x < b->size; x++) {
                int q = to_pos(x, y);
                if (b->cell[q] == EMPTY && b->pat4[att][q] == P4_FIVE) { *best = q; return 1; }
            }
    }
    int dfive = b->p4cnt[def][P4_FIVE];
    if (dfive >= 2) return 0;
    // 今 VCF で勝てるか
    TzZone z;
    if (dfive == 0 && tz_gate(b, att)) {
        int r = tz_probe_att(b, att, L.vcf_nodes, &z);
        if (r) { *best = z.first; return 1; }
    }
    if (depth <= 0) return 0;
    uint64_t key = b->hash ^ L2_SALT[att];
    L2Entry *e = &L.tt[key & (L2_TT_SIZE - 1)];
    if (e->key == key) {
        if (e->result == L2_WIN) { *best = e->best; return 1; }
        if (e->result == L2_FAIL && e->depth >= depth) return 0;
    }
    int cand[MAX_MOVES], sc[MAX_MOVES], nc = 0;
    if (dfive == 1) {
        for (int y = 0; y < b->size && !nc; y++)
            for (int x = 0; x < b->size; x++) {
                int q = to_pos(x, y);
                if (b->cell[q] == EMPTY && b->pat4[def][q] == P4_FIVE) { cand[nc] = q; sc[nc++] = 0; break; }
            }
    } else {
        int tmp[MAX_MOVES], nt = near_cells(b, tmp);
        int last = b->nmoves >= 2 ? b->moves[b->nmoves - 2] : -1;   // 攻め側の直前の手
        for (int i = 0; i < nt; i++) {
            int q = tmp[i], p4 = b->pat4[att][q];
            if (p4 < P4_BLOCK3 || p4 == P4_FIVE) continue;
            int s = p4 * 16;
            if (last >= 0) {
                int dx = abs(pos_x(q) - pos_x(last)), dy = abs(pos_y(q) - pos_y(last));
                int dd = dx > dy ? dx : dy;
                if (dd < 5) s += 5 - dd;
            }
            cand[nc] = q;
            sc[nc++] = s;
        }
        // 点の高い順
        for (int i = 1; i < nc; i++) {
            int m = cand[i], v = sc[i], j = i - 1;
            while (j >= 0 && sc[j] < v) { cand[j + 1] = cand[j]; sc[j + 1] = sc[j]; j--; }
            cand[j + 1] = m;
            sc[j + 1] = v;
        }
        if (nc > L2_MAX_CAND) nc = L2_MAX_CAND;
    }
    for (int i = 0; i < nc; i++) {
        int q = cand[i];
        if (!l2_legal(b, att, q)) continue;
        board_move_color(b, q, att);
        int ok = 0;
        if (!b->p4cnt[def][P4_FIVE] && tz_gate(b, att)) {
            TzZone zz;
            int r = tz_probe_att(b, att, L.vcf_nodes, &zz);
            if (r) ok = l2_and(b, depth, &zz, r == 2);
        }
        board_undo(b);
        if (L.aborted) return 0;
        if (ok) {
            e->key = key;
            e->result = L2_WIN;
            e->best = (int16_t)q;
            e->depth = (int16_t)depth;
            *best = q;
            return 1;
        }
    }
    if (!(e->key == key && e->result == L2_WIN)) {
        e->key = key;
        e->result = L2_FAIL;
        e->depth = (int16_t)depth;
        e->best = -1;
    }
    return 0;
}

int l2_solve(Board *b, int attacker, int max_depth, long max_nodes, long vcf_nodes, int *move, L2Result *res) {
    if (!ensure()) return 0;
    int ep = __atomic_load_n(&tz_epoch, __ATOMIC_RELAXED);
    if (!L.tt) {
        L.tt = calloc(L2_TT_SIZE, sizeof(L2Entry));
        if (!L.tt) return 0;
        L.epoch = ep;
    } else if (L.epoch != ep) {
        memset(L.tt, 0, sizeof(L2Entry) * L2_TT_SIZE);
        L.epoch = ep;
    }
    L.att = attacker;
    L.def = opp_color(attacker);
    L.forb = rule_has_forbidden(&b->rule);
    L.nodes = 0;
    L.max_nodes = max_nodes;
    L.vcf_nodes = vcf_nodes;
    L.aborted = 0;
    int win = 0, best = -1, d;
    for (d = 1; d <= max_depth && !win && !L.aborted; d++) win = l2_or(b, d, &best);
    if (move) *move = win ? best : -1;
    if (res) {
        res->nodes = L.nodes;
        res->aborted = L.aborted;
        res->depth = win ? d - 1 : 0;
    }
    return win;
}
