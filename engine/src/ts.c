// 評価で手を並べる αβ の脅威探索 (説明は ts.h)
#include "ts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "eval.h"
#include "tz.h"

long g_ts_vcf_nodes = 200;
long g_ts_quiet_nodes = 30;    // 静かな手の「パスしたら VCF」の確認のノード上限
int g_ts_threat_min = P4_BLOCK3;
int g_ts_loc = 3;
int g_ts_flags = 0;   // 調査用: 1 = 受け側の VCF の確認をしない
int g_ts_step = 1, g_ts_start = 1;   // 反復深化の刻みと最初の深さ

#define TTS_BITS 18
#define TTS_SIZE (1u << TTS_BITS)
#define MAX_CH 64

typedef struct {
    uint64_t key;
    int16_t depth;   // fail はこの深さまで成り立たない、win は 0x7fff
    int16_t move;
} TsEntry;

// 局面ごとの深さによらない結果 (反復深化で同じ局面を何度も展開するので): VCF がないこと、静かな脅威の手の一覧
#define QC_BITS 14
#define QC_SIZE (1u << QC_BITS)
#define QC_MOVES 30
typedef struct {
    uint64_t key;
    int8_t novcf;   // 1 = 攻め側に VCF なし (確かめ済み)
    int8_t nq;      // 静かな脅威の手の数 (-1 = まだ作っていない)
    int16_t mv[QC_MOVES];
} QEntry;

static _Thread_local uint16_t g_hint[NCELL];   // 前の証明の攻め側の手 (兄弟の手の確かめで先に読む)

static _Thread_local struct {
    QEntry *qc;
    TsEntry *tt;
    Board *b;
    int att, def, forb, root_n;
    long nodes, max_nodes;
    double deadline;
    int aborted;
} T;

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec * 1e-6;
}

static const uint64_t TSALT[3] = {0, 0x51ed270b2c8e93a7ULL, 0x8f1bbcdc6ed9eba1ULL};
#define TANDSALT 0x2545f4914f6cdd1dULL
static inline uint64_t tkey(int is_or) { return T.b->hash ^ TSALT[T.att] ^ (is_or ? 0 : TANDSALT); }

static inline int tlegal(int color, int q) {
    return !(T.forb && color == BLACK && board_is_forbidden(T.b, q));
}

static int find_p4(int color, int p4) {
    const Board *b = T.b;
    for (int q = 0; q < NCELL; q++)
        if (b->cell[q] == EMPTY && b->pat4[color][q] == p4) return q;
    return -1;
}

// 1 = 勝ち、0 = この深さでは勝てない、-1 = 調べていない
static int tt_get(uint64_t key, int depth, int *move) {
    TsEntry *e = &T.tt[key & (TTS_SIZE - 1)];
    if (e->key != key) return -1;
    if (e->depth == 0x7fff) { if (move) *move = e->move; return 1; }
    return e->depth >= depth ? 0 : -1;
}

static void tt_put(uint64_t key, int depth, int win, int move) {
    TsEntry *e = &T.tt[key & (TTS_SIZE - 1)];
    if (e->key == key && e->depth == 0x7fff && !win) return;
    e->key = key;
    e->depth = win ? 0x7fff : (int16_t)depth;
    e->move = (int16_t)move;
}

static inline int stop_now(void) {
    if (T.aborted) return 1;
    if (++T.nodes > T.max_nodes || (T.deadline > 0 && (T.nodes & 255) == 0 && now_ms() > T.deadline)) T.aborted = 1;
    return T.aborted;
}

static int near_attack(int q) {
    const Board *b = T.b;
    if (g_ts_loc <= 0 || b->nmoves - T.root_n < 2) return 1;
    for (int k = 0, i = b->nmoves - 2; k < 2 && i >= T.root_n; k++, i -= 2) {
        int m = b->moves[i];
        int dx = abs(pos_x(q) - pos_x(m)), dy = abs(pos_y(q) - pos_y(m));
        int d = dx > dy ? dx : dy;
        if (d <= 2 || ((dx == 0 || dy == 0 || dx == dy) && d <= g_ts_loc)) return 1;
    }
    return 0;
}

static int and_node(int depth, int ply);

static int or_node(int depth, int ply) {
    Board *b = T.b;
    int att = T.att, def = T.def;
    if (b->p4cnt[att][P4_FIVE]) return 1;
    if (stop_now()) return 0;
    int dfive = b->p4cnt[def][P4_FIVE];
    if (dfive >= 2) return 0;
    if (dfive == 1) {
        int q = find_p4(def, P4_FIVE);
        if (q < 0 || !tlegal(att, q)) return 0;
        board_move_color(b, q, att);
        int r = and_node(depth, ply + 1);
        board_undo(b);
        return r;
    }
    uint64_t key = tkey(1);
    int tm = -1, t = tt_get(key, depth, &tm);
    if (t >= 0) return t;
    QEntry *qe = &T.qc[key & (QC_SIZE - 1)];
    if (qe->key != key) { qe->key = key; qe->novcf = 0; qe->nq = -1; }
    if (!qe->novcf && tz_gate(b, att)) {
        TzZone z;
        int ok = tz_probe_att(b, att, g_ts_vcf_nodes, &z);
        if (ok && z.first >= 0) { tt_put(key, depth, 1, z.first); return 1; }
    }
    qe->novcf = 1;
    if (depth <= 0) return 0;
    // 攻め側の脅威の手: 先に四と三 (安い)、どれも勝てなければ打った後にパスしたら VCF がある手 (確かめが重い)。
    // それぞれ四を先に、あとは評価の高い順
    const int16_t *va = g_cell_value[att], *vd = g_cell_value[def];
    for (int phase = 0; phase < 2; phase++) {
        if (phase == 1 && g_ts_threat_min >= P4_FLEX3) break;
        int ch[MAX_CH], sc[MAX_CH], n = 0;
        if (phase == 1 && qe->key == key && qe->nq >= 0) {
            n = qe->nq;
            for (int i = 0; i < n; i++) ch[i] = qe->mv[i];
            goto search;
        }
        for (int q = 0; q < NCELL; q++) {
            if (b->cell[q] != EMPTY || !b->near[q]) continue;
            int p4 = b->pat4[att][q];
            if (p4 == P4_FIVE) continue;
            if (phase == 0 ? p4 < P4_FLEX3 : (p4 < g_ts_threat_min || p4 >= P4_FLEX3)) continue;
            if (p4 == P4_FLEX4) {
                if (tlegal(att, q)) { tt_put(key, depth, 1, q); return 1; }
                continue;
            }
            if (p4 < P4_BLOCK4 && !near_attack(q)) continue;
            if (!tlegal(att, q)) continue;
            if (phase == 1) {
                board_move_color(b, q, att);
                TzZone zq;
                int ok = tz_gate(b, att) && tz_probe_att(b, att, g_ts_quiet_nodes, &zq);
                board_undo(b);
                if (!ok) continue;
            }
            int s = va[board_pidx(b, att, q)] + vd[board_pidx(b, def, q)] / 2 + (p4 >= P4_BLOCK4 ? 1 << 20 : 0);
            if (q == tm) s += 1 << 24;
            if (g_hint[q]) s += (1 << 21) + g_hint[q];
            if (n >= MAX_CH) break;
            int j = n++;
            while (j > 0 && sc[j - 1] < s) { ch[j] = ch[j - 1]; sc[j] = sc[j - 1]; j--; }
            ch[j] = q;
            sc[j] = s;
        }
        if (phase == 1 && qe->key == key) {
            qe->nq = (int8_t)(n < QC_MOVES ? n : QC_MOVES);
            for (int i = 0; i < qe->nq; i++) qe->mv[i] = (int16_t)ch[i];
        }
    search:
        for (int i = 0; i < n; i++) {
            board_move_color(b, ch[i], att);
            int r = and_node(depth - 1, ply + 1);
            board_undo(b);
            if (T.aborted) return 0;
            if (r) { tt_put(key, depth, 1, ch[i]); return 1; }
        }
    }
    tt_put(key, depth, 0, -1);
    return 0;
}

static int zone_has(const TzZone *z, int q) {
    for (int i = 0; i < z->n; i++)
        if (z->cells[i] == q) return 1;
    return 0;
}

static int and_node(int depth, int ply) {
    Board *b = T.b;
    int att = T.att, def = T.def;
    if (b->p4cnt[def][P4_FIVE]) return 0;
    if (stop_now()) return 0;
    int na = b->p4cnt[att][P4_FIVE];
    if (na >= 2) return 1;
    if (na == 1) {
        int f = find_p4(att, P4_FIVE);
        if (f < 0) return 0;
        if (!tlegal(def, f)) return 1;
        board_move_color(b, f, def);
        int r = or_node(depth, ply + 1);
        board_undo(b);
        return r;
    }
    uint64_t key = tkey(0);
    int t = tt_get(key, depth, NULL);
    if (t >= 0) return t;
    // 攻め側の脅威の圏: 達四の点ごとの手順の圏の共通部分、なければパスしたときの VCF の圏
    TzZone acc, z;
    acc.n = 0;
    int have = 0, all = 0;
    for (int q = 0; q < NCELL; q++) {
        if (b->cell[q] != EMPTY || b->pat4[att][q] != P4_FLEX4 || !tlegal(att, q)) continue;
        if (tz_line_zone(b, att, &q, 1, &z) != 1) continue;
        if (!have) { acc = z; have = 1; continue; }
        int m = 0;
        for (int k = 0; k < acc.n; k++)
            if (zone_has(&z, acc.cells[k])) acc.cells[m++] = acc.cells[k];
        acc.n = m;
    }
    if (!have) {
        if (!tz_gate(b, att)) { tt_put(key, depth, 0, -1); return 0; }
        int r = tz_probe_att(b, att, g_ts_vcf_nodes, &z);
        if (!r) { tt_put(key, depth, 0, -1); return 0; }
        if (r == 2) all = 1;
        else acc = z;
    }
    if (!(g_ts_flags & 1) && tz_gate(b, def)) {
        TzZone zd;
        int dv = tz_probe_att(b, def, g_ts_vcf_nodes, &zd);
        if (dv) { tt_put(key, 0x7ffe, 0, -1); return 0; }
    }
    static _Thread_local uint8_t seen[NCELL];
    int ch[MAX_CH], sc[MAX_CH], n = 0;
    const int16_t *va = g_cell_value[att], *vd = g_cell_value[def];
    for (int q = 0; q < NCELL; q++) {
        if (b->cell[q] != EMPTY || !b->near[q]) continue;
        int inz = all || b->pat4[def][q] >= P4_BLOCK4;
        if (!inz) {
            for (int k = 0; k < acc.n; k++)
                if (acc.cells[k] == q) { inz = 1; break; }
        }
        if (!inz || !tlegal(def, q) || seen[q]) continue;
        if (n >= MAX_CH) { for (int k = 0; k < n; k++) seen[ch[k]] = 0; tt_put(key, depth, 0, -1); return 0; }
        // 受けの強そうな順 (反証が早く見つかるように): 相手の点を消す価値 + 自分の形
        int s = va[board_pidx(b, att, q)] + vd[board_pidx(b, def, q)] / 2;
        seen[q] = 1;
        int j = n++;
        while (j > 0 && sc[j - 1] < s) { ch[j] = ch[j - 1]; sc[j] = sc[j - 1]; j--; }
        ch[j] = q;
        sc[j] = s;
    }
    for (int k = 0; k < n; k++) seen[ch[k]] = 0;
    for (int i = 0; i < n; i++) {
        board_move_color(b, ch[i], def);
        int r = or_node(depth, ply + 1);
        board_undo(b);
        if (T.aborted) return 0;
        if (!r) { tt_put(key, depth, 0, -1); return 0; }
    }
    tt_put(key, depth, 1, -1);
    return 1;
}

static int run(Board *b, int attacker, int max_depth, long max_nodes, int max_ms, TsResult *res);

static int ts_alloc(void) {
    if (!T.tt) {
        T.tt = calloc(TTS_SIZE, sizeof(TsEntry));
        T.qc = calloc(QC_SIZE, sizeof(QEntry));
    }
    return T.tt && T.qc;
}

int ts_win(Board *b, int attacker, int max_depth, long max_nodes, int max_ms, TsResult *res) {
    if (!ts_alloc()) return 0;
    memset(T.tt, 0, TTS_SIZE * sizeof(TsEntry));
    memset(T.qc, 0, QC_SIZE * sizeof(QEntry));   // fail の深さは呼び出しの条件 (予算) によるので毎回消す
    return run(b, attacker, max_depth, max_nodes, max_ms, res);
}

int ts_probe(Board *b, int attacker, int max_depth, long max_nodes, TsResult *res) {
    if (!ts_alloc()) return 0;
    return run(b, attacker, max_depth, max_nodes, 0, res);
}

static int run(Board *b, int attacker, int max_depth, long max_nodes, int max_ms, TsResult *res) {
    T.b = b;
    T.att = attacker;
    T.def = opp_color(attacker);
    T.forb = rule_has_forbidden(&b->rule);
    T.root_n = b->nmoves;
    T.nodes = 0;
    T.max_nodes = max_nodes;
    T.deadline = max_ms > 0 ? now_ms() + max_ms : 0;
    T.aborted = 0;
    int win = 0, d = g_ts_start, move = -1;
    // 攻め側が手番でないとき (パスしたら) は根を受け側の局面として扱わず、攻め側の手番として調べる
    for (; d <= max_depth && !T.aborted; d += g_ts_step) {
        if (or_node(d, 0)) { win = 1; break; }
    }
    if (win) {
        TsEntry *e = &T.tt[tkey(1) & (TTS_SIZE - 1)];
        move = e->key == tkey(1) ? e->move : -1;
    }
    if (res) {
        res->win = win;
        res->move = move;
        res->depth = win ? d : d - 1;
        res->nodes = T.nodes;
        res->aborted = T.aborted;
    }
    return win;
}

// ---- 証明の圏 (ts_zone) ----
static _Thread_local struct {
    uint8_t mark[NCELL];
    int *out, n, max, fail;
    long steps;
    uint64_t seen[4096];
} Z;

static void zadd(int q) {
    if (q < 0 || q >= NCELL || Z.mark[q]) return;
    if (Z.n >= Z.max) { Z.fail = 1; return; }
    Z.mark[q] = 1;
    Z.out[Z.n++] = q;
}

static void zadd_zone(const TzZone *z) {
    for (int i = 0; i < z->n; i++) zadd(z->cells[i]);
    zadd(z->first);
}

static int zseen(uint64_t key) {
    uint64_t *s = &Z.seen[key & 4095];
    if (*s == key) return 1;
    *s = key;
    return 0;
}

static void zwalk(int is_or, int ply) {
    Board *b = T.b;
    int att = T.att, def = T.def;
    if (Z.fail) return;
    if (++Z.steps > 30000 || ply > 120) { Z.fail = 5; return; }
    if (zseen(tkey(is_or))) return;
    if (is_or) {
        if (b->p4cnt[att][P4_FIVE]) { zadd(find_p4(att, P4_FIVE)); return; }
        if (b->p4cnt[def][P4_FIVE] == 1) {
            int q = find_p4(def, P4_FIVE);
            zadd(q);
            board_move_color(b, q, att);
            zwalk(0, ply + 1);
            board_undo(b);
            return;
        }
        if (tz_gate(b, att)) {
            TzZone z;
            int r = tz_probe_att(b, att, 20000, &z);
            if (r == 1) { zadd_zone(&z); return; }
            // r == 2 (止める点が禁じ手で圏を絞れない VCF): 置換表の手をたどる
        }
        TsEntry *e = &T.tt[tkey(1) & (TTS_SIZE - 1)];
        int m = e->key == tkey(1) && e->depth == 0x7fff ? e->move : -1;
        if (m < 0) {
            // 圏の作り直しで受けの候補が探索の時と少し違うことがある (VCF の手順の再利用による)。その局面だけ読み直す
            T.aborted = 0;
            T.max_nodes = T.nodes + 20000;
            T.deadline = 0;
            for (int d = 1; d <= 12 && !T.aborted; d++)
                if (or_node(d, ply)) break;
            m = e->key == tkey(1) && e->depth == 0x7fff ? e->move : -1;
            if (m < 0 && b->p4cnt[att][P4_FIVE] == 0) {
                // 勝ちが VCF (TT に手がない形) のこともある
                TzZone z;
                int r2 = tz_gate(b, att) ? tz_probe_att(b, att, 20000, &z) : 0;
                if (r2 == 1) { zadd_zone(&z); return; }
                if (r2 == 2) { Z.fail = 6; return; }
            }
        }
        if (m < 0 || b->cell[m] != EMPTY) { Z.fail = 3; return; }
        if (g_hint[m] < 60000) g_hint[m]++;
        zadd(m);
        board_move_color(b, m, att);
        zwalk(0, ply + 1);
        board_undo(b);
        return;
    }
    int na = b->p4cnt[att][P4_FIVE];
    if (na >= 2) {
        for (int q = 0; q < NCELL; q++)
            if (b->cell[q] == EMPTY && b->pat4[att][q] == P4_FIVE) zadd(q);
        return;
    }
    if (na == 1) {
        int f = find_p4(att, P4_FIVE);
        zadd(f);
        if (!tlegal(def, f)) return;
        board_move_color(b, f, def);
        zwalk(1, ply + 1);
        board_undo(b);
        return;
    }
    TzZone acc, z;
    acc.n = 0;
    int have = 0;
    for (int q = 0; q < NCELL; q++) {
        if (b->cell[q] != EMPTY || b->pat4[att][q] != P4_FLEX4 || !tlegal(att, q)) continue;
        if (tz_line_zone(b, att, &q, 1, &z) != 1) continue;
        zadd_zone(&z);   // 各達四の手順の圏 (外に受け側の石があっても、その達四で勝つ)
        if (!have) { acc = z; have = 1; continue; }
        int m = 0;
        for (int k = 0; k < acc.n; k++)
            if (zone_has(&z, acc.cells[k])) acc.cells[m++] = acc.cells[k];
        acc.n = m;
    }
    if (!have) {
        int r = tz_gate(b, att) ? tz_probe_att(b, att, g_ts_vcf_nodes, &z) : 0;
        if (r != 1) { Z.fail = 2; return; }
        acc = z;
        zadd_zone(&z);
    }
    int ch[MAX_CH], n = 0;
    for (int q = 0; q < NCELL; q++) {
        if (b->cell[q] != EMPTY || !b->near[q]) continue;
        int inz = b->pat4[def][q] >= P4_BLOCK4 || zone_has(&acc, q);
        if (!inz || !tlegal(def, q)) continue;
        if (n >= MAX_CH) { Z.fail = 4; return; }
        ch[n++] = q;
    }
    for (int i = 0; i < n && !Z.fail; i++) {
        zadd(ch[i]);
        board_move_color(b, ch[i], def);
        zwalk(1, ply + 1);
        board_undo(b);
    }
}

int ts_zone(Board *b, int attacker, int *cells, int max) {
    if (!T.tt || T.b != b || T.att != attacker) return -1;
    memset(Z.mark, 0, sizeof Z.mark);
    memset(Z.seen, 0, sizeof Z.seen);
    Z.out = cells;
    Z.n = 0;
    Z.max = max;
    Z.fail = 0;
    Z.steps = 0;
    T.root_n = b->nmoves;
    zwalk(1, 0);
    if (Z.fail && getenv("TS_ZDEBUG")) fprintf(stderr, "ts_zone fail %d (steps %ld, n %d)\n", Z.fail, Z.steps, Z.n);
    return Z.fail ? -1 : Z.n;
}

void ts_hint_clear(void) { memset(g_hint, 0, sizeof g_hint); }
