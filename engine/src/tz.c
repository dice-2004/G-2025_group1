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
