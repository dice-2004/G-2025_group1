// 脅威の潜在力 (説明は tp.h)
#include "tp.h"

#include <string.h>

#include "tz.h"

int g_tp_own = 40, g_tp_opp = 100, g_tp_lo = P4_BLOCK3, g_tp_cap = 20;
long g_tp_nodes = 30;
int g_tpv_own = 4000, g_tpv_opp = 1500;
long g_tpv_nodes = 100;
_Thread_local long g_tp_calls, g_tp_hits;

#define TP_BITS 14
#define TP_SIZE (1u << TP_BITS)

#define TP_CELLS 12

typedef struct {
    uint64_t key;
    uint8_t n[3];   // [色] の点の数
} TpEntry;

typedef struct {
    uint64_t key;
    uint8_t n[3];
    int16_t cells[3][TP_CELLS];   // [色] の点 (TP_CELLS 個まで)
} TpCells;

static _Thread_local TpCells tpo[1u << 13];
static _Thread_local int tpo_epoch;

static _Thread_local TpEntry tpc[TP_SIZE];
static int tp_epoch;
static _Thread_local int tp_my_epoch;

void tp_clear(void) { __atomic_add_fetch(&tp_epoch, 1, __ATOMIC_RELAXED); }

static int tp_list(Board *b, int color, int lo, long nodes, int cap, int16_t *out) {
    int n = 0;
    int forb = color == BLACK && rule_has_forbidden(&b->rule);
    for (int y = 0; y < b->size && n < cap; y++)
        for (int x = 0; x < b->size && n < cap; x++) {
            int q = to_pos(x, y);
            if (b->cell[q] != EMPTY || !b->near[q]) continue;
            int p4 = b->pat4[color][q];
            if (p4 < lo || p4 >= P4_FLEX3) continue;
            if (forb && board_is_forbidden(b, q)) continue;
            if (tz_probe_extra(b, color, q, nodes)) {
                if (out) out[n] = (int16_t)q;
                n++;
            }
        }
    return n;
}

int tp_count(Board *b, int color, int lo, long nodes, int cap) { return tp_list(b, color, lo, nodes, cap, NULL); }

static _Thread_local TpCells *cur;
static _Thread_local int cur_me;

int tp_prepare(Board *b) {
    int ep = __atomic_load_n(&tp_epoch, __ATOMIC_RELAXED);
    if (ep != tpo_epoch) {
        memset(tpo, 0, sizeof tpo);
        tpo_epoch = ep;
    }
    int me = board_side(b), op = opp_color(me);
    uint64_t key = b->hash ^ ((uint64_t)g_tp_lo << 56) ^ ((uint64_t)g_tp_nodes << 40);
    TpCells *e = &tpo[key & ((1u << 13) - 1)];
    g_tp_calls++;
    if (e->key != key) {
        e->key = key;
        e->n[me] = (uint8_t)tp_list(b, me, g_tp_lo, g_tp_nodes, TP_CELLS, e->cells[me]);
        e->n[op] = (uint8_t)tp_list(b, op, g_tp_lo, g_tp_nodes, TP_CELLS, e->cells[op]);
    } else {
        g_tp_hits++;
    }
    cur = e;
    cur_me = me;
    return e->n[me] + e->n[op];
}

int tp_mark(int q) {
    int r = 0, op = opp_color(cur_me);
    for (int k = 0; k < cur->n[cur_me]; k++)
        if (cur->cells[cur_me][k] == q) { r |= 1; break; }
    for (int k = 0; k < cur->n[op]; k++)
        if (cur->cells[op][k] == q) { r |= 2; break; }
    return r;
}

int tp_correction(Board *b) {
    int ep = __atomic_load_n(&tp_epoch, __ATOMIC_RELAXED);
    if (ep != tp_my_epoch) {
        memset(tpc, 0, sizeof tpc);
        tp_my_epoch = ep;
    }
    int me = board_side(b), op = opp_color(me);
    // 数え方の設定もキーに混ぜる (INFO で変えたら別の値)
    uint64_t key = b->hash ^ ((uint64_t)g_tp_lo << 56) ^ ((uint64_t)g_tp_nodes << 40) ^ ((uint64_t)g_tp_cap << 32);
    TpEntry *e = &tpc[key & (TP_SIZE - 1)];
    g_tp_calls++;
    if (e->key != key) {
        e->key = key;
        e->n[me] = (uint8_t)tp_count(b, me, g_tp_lo, g_tp_nodes, g_tp_cap);
        e->n[op] = (uint8_t)tp_count(b, op, g_tp_lo, g_tp_nodes, g_tp_cap);
    } else {
        g_tp_hits++;
    }
    return e->n[me] * g_tp_own - e->n[op] * g_tp_opp;
}

// 旗の表 (局面のハッシュで覚える): 0 = 未調査、1 = なし、2 = 自分の VCF、3 = 相手の VCF
#define TV_BITS 16
static _Thread_local struct { uint64_t key; uint8_t f; } tvc[1u << TV_BITS];
static _Thread_local int tv_epoch;
_Thread_local long g_tpv_probes, g_tpv_hits;

int tp_vcf_term(Board *b) {
    int ep = __atomic_load_n(&tp_epoch, __ATOMIC_RELAXED);
    if (ep != tv_epoch) {
        memset(tvc, 0, sizeof tvc);
        tv_epoch = ep;
    }
    uint64_t key = b->hash ^ ((uint64_t)g_tpv_nodes << 48) ^ ((uint64_t)(g_tpv_own != 0) << 63);
    unsigned i = key & ((1u << TV_BITS) - 1);
    int f;
    g_tpv_probes++;
    if (tvc[i].key == key && tvc[i].f) {
        g_tpv_hits++;
        f = tvc[i].f;
    } else {
        int me = board_side(b), op = opp_color(me);
        TzZone z;
        f = 1;
        if (g_tpv_own && tz_gate(b, me) && tz_probe_att(b, me, g_tpv_nodes, &z)) f = 2;
        else if (tz_gate(b, op) && tz_probe_att(b, op, g_tpv_nodes, &z)) f = 3;
        tvc[i].key = key;
        tvc[i].f = (uint8_t)f;
    }
    return f == 2 ? g_tpv_own : f == 3 ? -g_tpv_opp : 0;
}

int tp_leaf_eval(Board *b, int stat, int alpha, int beta) {
    int up = g_tpv_own, down = g_tpv_opp;
    if (stat - down >= beta) return stat;        // 下げても窓の上 (上げても同じ)
    if (stat + up <= alpha) return stat;         // 上げても窓の下
    return stat + tp_vcf_term(b);
}
