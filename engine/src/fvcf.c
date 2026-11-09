// VCF 専用の軽い盤による VCF (説明は fvcf.h)
//
// vcf.c の attacker_node / defender_node (TSS_VCF) と walk_line を、同じ順序で重ね盤の上に写したもの。
// 違うのは盤の持ち方だけ:
//   - 石は自前の cell[] に置く。key (線の並び) と攻め側の線パターンは、石の周りの触ったマスだけ盤から写して差分更新する
//     (触っていないマスは盤 b の値がそのまま正しい。重ね盤は空点に石を足すだけで、盤の石は取らないため)。
//   - 攻め側の「四になる点」(pat4 が BLOCK4..FLEX4) は集合で持つ。vcf.c は盤を順に走査して同じ点を集めるので、
//     (点数, 盤の順) で並べれば同じ候補の列になる。
//   - 五の点は直前の手の周りの線だけを見る。探索中に攻め側の五の点は残らず (あれば打っている・止められている)、
//     受け側の五の点は受け側の直前の手を通る線にしかできないので、vcf.c の fives_near / find_fives と同じ点になる。
//   - ハッシュは盤と同じ Zobrist 値 (盤に石を置いてハッシュの差から読む) なので、置換表の番地も vcf.c と同じ。
#include "fvcf.h"

#include "tz.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define TT_BITS 20
#define TT_SIZE (1u << TT_BITS)
#define MAX_CAND 128
#define MAX_THREADS 16
#define MAX_OV 512

int g_fvcf = 1;
_Thread_local long g_fvcf_forbid[3];

enum { E_NONE = 0, E_WIN = 1, E_FAIL = 2 };

typedef struct {
    uint64_t key;
    int16_t depth;
    int16_t best;
    int8_t result;
} Ent;

static Ent *tts[MAX_THREADS];
static int tt_epoch[MAX_THREADS];
static int fv_epoch;
static _Thread_local int tid;
static _Thread_local Ent *tt;

// vcf.c の SALT[TSS_VCF] と同じ値
static const uint64_t SALT[3] = {0, 0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL};

static uint64_t Z[3][NCELL];
static pthread_once_t z_once = PTHREAD_ONCE_INIT;

// 盤の Zobrist 値を、空の盤に 1 つずつ石を置いたときのハッシュの差として読む (board.c の表は static のため)
static void z_init(void) {
    static Board s;
    Rule r = {0};
    board_init(&s, MAX_SIZE, &r);
    for (int y = 0; y < MAX_SIZE; y++)
        for (int x = 0; x < MAX_SIZE; x++) {
            int p = to_pos(x, y);
            for (int c = BLACK; c <= WHITE; c++) {
                uint64_t h0 = s.hash;
                board_move_color(&s, p, c);
                Z[c][p] = s.hash ^ h0;
                board_undo(&s);
            }
        }
}

typedef struct {
    Board *b;
    int att, def, restricted;
    long nodes, max_nodes;
    int aborted, root_best;
    uint64_t hash;
    uint32_t gen;
    int ov[MAX_OV];                 // 重ね盤に打った手
    int nov;
    // 根の五の点 (盤 b から)
    int root_af, root_nd, root_df[2];
    uint8_t cell[NCELL];
    uint32_t st[NCELL];             // st[q] == gen なら key/pa/p4a は重ね盤の値
    uint32_t key[NCELL][4];
    uint8_t pa[NCELL][4];           // 攻め側の線パターン
    uint8_t p4a[NCELL];
    int useset;                     // 0 = 四の点の集合を保たない (手順の再生)
    uint32_t zgen, zst[NCELL];      // 圏の点の重複除け
    uint32_t fs[NCELL];             // fs[q] == gen なら q は四の点の集合に入っている
    int16_t fidx[NCELL];
    int16_t fl[NCELL];
    int nf;
} L;

static _Thread_local L S;

void fvcf_clear(void) {
    __atomic_add_fetch(&fv_epoch, 1, __ATOMIC_RELAXED);
}

void fvcf_set_thread(int id) { tid = id < 0 || id >= MAX_THREADS ? 0 : id; }

// useset = 1 (VCF の探索): key と攻め側のパターン・四の点の集合を保つ。0 (手順の再生): key だけを保つ
static inline __attribute__((always_inline)) void touch(int q, const int vcf) {
    if (S.st[q] == S.gen) return;
    const Board *b = S.b;
    S.st[q] = S.gen;
    memcpy(S.key[q], b->key[q], sizeof S.key[q]);
    if (!vcf) return;
    memcpy(S.pa[q], b->pat[S.att][q], 4);
    S.p4a[q] = b->pat4[S.att][q];
}

// color が空点 q に打ったときの方向 d の線パターン
static inline int pat(int color, int q, int d) {
    if (S.st[q] != S.gen) return S.b->pat[color][q][d];
    return color == S.att && S.useset ? S.pa[q][d] : g_pattern_table[color][S.key[q][d]];
}

static inline int p4att(int q) { return S.st[q] == S.gen ? S.p4a[q] : S.b->pat4[S.att][q]; }

static inline int is_four(int p4) { return p4 >= P4_BLOCK4 && p4 <= P4_FLEX4; }

static inline void set_add(int q) {
    S.fs[q] = S.gen;
    S.fidx[q] = (int16_t)S.nf;
    S.fl[S.nf++] = (int16_t)q;
}

static inline void set_del(int q) {
    int i = S.fidx[q], last = S.fl[--S.nf];
    S.fl[i] = (int16_t)last;
    S.fidx[last] = (int16_t)i;
    S.fs[q] = 0;
}

static inline void set_update(int q) {
    int in = S.fs[q] == S.gen, want = is_four(S.p4a[q]);
    if (want && !in) set_add(q);
    else if (!want && in) set_del(q);
}

// 空点 q の方向 d の攻め側のパターンを key から作り直す
static inline void refresh(int q, int d) {
    uint8_t np = g_pattern_table[S.att][S.key[q][d]];
    if (np == S.pa[q][d]) return;
    S.pa[q][d] = np;
    S.p4a[q] = g_p4_table[pattern_index(S.pa[q])];
    set_update(q);
}

static inline __attribute__((always_inline)) void put_m(int p, int color, const int vcf) {
    touch(p, vcf);
    if (vcf && S.fs[p] == S.gen) set_del(p);
    S.cell[p] = (uint8_t)color;
    S.hash ^= Z[color][p];
    S.ov[S.nov++] = p;
    for (int d = 0; d < 4; d++) {
        int off = DIR_OFFSET[d];
        for (int k = 1; k <= 5; k++) {
            int q = p + k * off;
            if (S.cell[q] != WALL) {
                touch(q, vcf);
                S.key[q][d] |= (uint32_t)color << (2 * key_slot(-k));
                if (vcf && S.cell[q] == EMPTY) refresh(q, d);
            }
            q = p - k * off;
            if (S.cell[q] != WALL) {
                touch(q, vcf);
                S.key[q][d] |= (uint32_t)color << (2 * key_slot(k));
                if (vcf && S.cell[q] == EMPTY) refresh(q, d);
            }
        }
    }
}

static inline __attribute__((always_inline)) void undo_m(const int vcf) {
    int p = S.ov[--S.nov];
    int color = S.cell[p];
    S.cell[p] = EMPTY;
    S.hash ^= Z[color][p];
    for (int d = 0; d < 4; d++) {
        int off = DIR_OFFSET[d];
        for (int k = 1; k <= 5; k++) {
            int q = p + k * off;
            if (S.cell[q] != WALL) {
                S.key[q][d] &= ~(3u << (2 * key_slot(-k)));
                if (vcf && S.cell[q] == EMPTY) refresh(q, d);
            }
            q = p - k * off;
            if (S.cell[q] != WALL) {
                S.key[q][d] &= ~(3u << (2 * key_slot(k)));
                if (vcf && S.cell[q] == EMPTY) refresh(q, d);
            }
        }
    }
    if (!vcf) return;
    // 石のあったマスは置いている間パターンを保っていないので作り直す
    for (int d = 0; d < 4; d++) S.pa[p][d] = g_pattern_table[S.att][S.key[p][d]];
    S.p4a[p] = g_p4_table[pattern_index(S.pa[p])];
    set_update(p);
}

static void put_v(int p, int color) { put_m(p, color, 1); }
static void undo_v(void) { undo_m(1); }
static void put_r(int p, int color) { put_m(p, color, 0); }
static void undo_r(void) { undo_m(0); }
static void put(int p, int color) { if (S.useset) put_v(p, color); else put_r(p, color); }
static void undo(void) { if (S.useset) undo_v(); else undo_r(); }

// m を通る線で、color が打つとその線の方向に五になる空点 (m の周り ±4、方向・距離の順)。
// m を含む ±4 に color の石が (m を入れて) 4 つなければその線に五の点はないので表を引かない
static int line_fives(int color, int m, int *out, int max) {
    int n = 0;
    for (int d = 0; d < 4; d++) {
        int off = DIR_OFFSET[d], c = 0;
        for (int k = -4; k <= 4; k++) c += S.cell[m + k * off] == color;
        if (c < 4) continue;
        for (int k = -4; k <= 4; k++) {
            int q = m + k * off;
            if (k == 0 || S.cell[q] != EMPTY || pat(color, q, d) != P_F5) continue;
            out[n++] = q;
            if (n >= max) return n;
        }
    }
    return n;
}

// board.c の forbidden_rec と同じ (重ね盤の上で)。戻り値 0 = 禁じ手でない、1 = 長連、2 = 四四、3 = 三三
static int forb_rec(int p, int depth) {
    const Rule *r = &S.b->rule;
    int pt[4], f3dirs[4], nf3 = 0, fours = 0;
    for (int d = 0; d < 4; d++) pt[d] = pat(BLACK, p, d);
    for (int d = 0; d < 4; d++)
        if (pt[d] == P_F5) return 0;
    for (int d = 0; d < 4; d++) {
        switch (pt[d]) {
        case P_OL: if (r->forbid_overline) return 1; break;
        case P_B4: case P_F4: fours += 1; break;
        case P_D4: fours += 2; break;
        case P_F3: f3dirs[nf3++] = d; break;
        default: break;
        }
    }
    if (r->forbid_44 && fours >= 2) return 2;
    if (!r->forbid_33 || nf3 < 2) return 0;
    if (depth > 8) return 0;
    put(p, BLACK);
    int real = 0;
    for (int i = 0; i < nf3 && real < 2; i++) {
        int d = f3dirs[i], off = DIR_OFFSET[d];
        int ok = 0;
        for (int k = -4; k <= 4 && !ok; k++) {
            if (k == 0) continue;
            int q = p + k * off;
            if (S.cell[q] != EMPTY || pat(BLACK, q, d) != P_F4) continue;
            int step = k > 0 ? -1 : 1, contiguous = 1;
            for (int m = k + step; m != 0; m += step)
                if (S.cell[p + m * off] != BLACK) { contiguous = 0; break; }
            if (!contiguous) continue;
            if (!forb_rec(q, depth + 1)) ok = 1;
        }
        real += ok;
    }
    undo();
    return real >= 2 ? 3 : 0;
}

static inline int legal(int color, int p) {
    if (color != BLACK || !S.restricted || S.cell[p] != EMPTY) return 1;
    int f = forb_rec(p, 0);
    if (f) g_fvcf_forbid[f - 1]++;
    return !f;
}

// m を通る線の color の五の点 (m の周り ±4)。同じ点は 1 回
static int fives_near(int color, int m, int *out, int max) {
    int n = 0;
    for (int d = 0; d < 4; d++) {
        int off = DIR_OFFSET[d];
        if (color != S.att) {
            // 受け側: m を含む ±4 に color の石が 4 つ (m を入れて) なければこの線に五の点はない
            int c = 0;
            for (int k = -4; k <= 4; k++) c += S.cell[m + k * off] == color;
            if (c < 4) continue;
        }
        for (int k = -4; k <= 4; k++) {
            int q = m + k * off;
            if (k == 0 || S.cell[q] != EMPTY || pat(color, q, d) != P_F5) continue;
            int dup = 0;
            for (int i = 0; i < n; i++) dup |= out[i] == q;
            if (dup) continue;
            out[n++] = q;
            if (n >= max) return n;
        }
    }
    return n;
}

static int chebyshev(int p, int q) {
    int dx = abs(pos_x(p) - pos_x(q)), dy = abs(pos_y(p) - pos_y(q));
    return dx > dy ? dx : dy;
}

// k 手前 (1 = 直前) の手。重ね盤の手、なければ盤の履歴
static inline int move_back(int k) {
    if (k <= S.nov) return S.ov[S.nov - k];
    return S.b->moves[S.b->nmoves - (k - S.nov)];
}

static int near_bonus(int p) {
    int s = 0, total = S.b->nmoves + S.nov;
    for (int k = 1; k <= 2 && k <= total; k++) {
        int d = chebyshev(p, move_back(k));
        if (d < 6) s += 6 - d;
    }
    return s;
}

static void tt_store(uint64_t key, int depth, int win, int best) {
    if (S.aborted) return;
    Ent *e = &tt[key & (TT_SIZE - 1)];
    if (e->key == key && e->result == E_WIN && !win) return;
    e->key = key;
    e->depth = (int16_t)depth;
    e->best = (int16_t)best;
    e->result = win ? E_WIN : E_FAIL;
}

static int win_at(uint64_t key, int depth, int best) {
    tt_store(key, depth, 1, best);
    if (S.nov == 0) S.root_best = best;
    return 1;
}

static int d_node(int depth);

static int a_node(int depth) {
    if (++S.nodes > S.max_nodes) { S.aborted = 1; return 0; }
    int f[2], ndef;
    uint64_t key = S.hash ^ SALT[S.att];
    if (S.nov == 0) {
        if (S.root_af >= 0) return win_at(key, depth, S.root_af);
        ndef = S.root_nd;
        f[0] = S.root_df[0];
    } else {
        ndef = fives_near(S.def, S.ov[S.nov - 1], f, 2);
    }
    if (ndef >= 2) return 0;

    Ent *e = &tt[key & (TT_SIZE - 1)];
    if (e->key == key) {
        if (e->result == E_WIN) {
            if (S.nov == 0) S.root_best = e->best;
            return 1;
        }
        if (e->depth >= depth) return 0;
    }

    if (ndef == 1) {
        int q = f[0];
        if (depth <= 0 || !legal(S.att, q)) { tt_store(key, depth, 0, -1); return 0; }
        put_v(q, S.att);
        int r = d_node(depth - 1);
        undo_v();
        if (r) return win_at(key, depth, q);
        tt_store(key, depth, 0, -1);
        return 0;
    }

    // 四の点: 達四 (盤の順で最初の打てる点) なら勝ち、ほかは候補
    int cand[S.nf + 1], sc[S.nf + 1], flex[S.nf + 1], nc = 0, nfx = 0;
    for (int i = 0; i < S.nf; i++) {
        int p = S.fl[i];
        if (p4att(p) == P4_FLEX4) flex[nfx++] = p;
        else cand[nc++] = p;
    }
    if (nfx) {
        // 盤の順 (小さい番号から) に打てるか確かめる
        for (int i = 1; i < nfx; i++) {
            int m = flex[i], j = i - 1;
            while (j >= 0 && flex[j] > m) { flex[j + 1] = flex[j]; j--; }
            flex[j + 1] = m;
        }
        for (int i = 0; i < nfx; i++)
            if (legal(S.att, flex[i])) return win_at(key, depth, flex[i]);
    }
    if (depth <= 0) return 0;

    if (nc > MAX_CAND) {
        // vcf.c は盤の順で最初の MAX_CAND 個だけを使う
        for (int i = 1; i < nc; i++) {
            int m = cand[i], j = i - 1;
            while (j >= 0 && cand[j] > m) { cand[j + 1] = cand[j]; j--; }
            cand[j + 1] = m;
        }
        nc = MAX_CAND;
    }
    // (点数の高い順, 同点は盤の順)
    for (int i = 0; i < nc; i++) {
        int p = cand[i];
        sc[i] = (p4att(p) * 32 + near_bonus(p) + 512) * 1024 + (1023 - p);
    }
    for (int i = 1; i < nc; i++) {
        int m = cand[i], s = sc[i], j = i - 1;
        while (j >= 0 && sc[j] < s) { cand[j + 1] = cand[j]; sc[j + 1] = sc[j]; j--; }
        cand[j + 1] = m;
        sc[j + 1] = s;
    }
    for (int i = 0; i < nc; i++) {
        int p = cand[i];
        if (!legal(S.att, p)) continue;
        put_v(p, S.att);
        int r = d_node(depth - 1);
        undo_v();
        if (r) return win_at(key, depth, p);
        if (S.aborted) return 0;
    }
    tt_store(key, depth, 0, -1);
    return 0;
}

static int d_node(int depth) {
    if (++S.nodes > S.max_nodes) { S.aborted = 1; return 0; }
    int f[2];
    int natt = fives_near(S.att, S.ov[S.nov - 1], f, 2);
    if (natt >= 2) return 1;
    if (natt == 1) {
        int q = f[0];
        if (!legal(S.def, q)) return 1;
        put_v(q, S.def);
        int r = a_node(depth);
        undo_v();
        return r;
    }
    return 0;
}

// vcf.c の walk_line (TSS_VCF) と同じ手順
static int walk(int first, int *line, int max) {
    int n = 0, played = 0;
    int df[2], nd = S.root_nd < 2 ? S.root_nd : 2;
    df[0] = S.root_df[0];
    df[1] = S.root_df[1];
    while (n < max - 1) {
        int f[2], q = -1;
        if (played == 0 && S.root_af >= 0) { line[n++] = S.root_af; break; }
        if (nd == 1) q = df[0];
        else if (n == 0) q = first;
        else {
            uint64_t key = S.hash ^ SALT[S.att];
            Ent *e = &tt[key & (TT_SIZE - 1)];
            if (e->key == key && e->result == E_WIN && e->best >= 0) q = e->best;
        }
        if (q < 0 || S.cell[q] != EMPTY) break;
        line[n++] = q;
        put_v(q, S.att);
        played++;
        // 受け側: 攻め側の五の点 (今の手の周り)、受け側の五の点 (残っていれば打ち切り)
        int left = 0;
        for (int i = 0; i < nd; i++) left += df[i] != q;
        int na = fives_near(S.att, q, f, 2);
        if (na >= 2 || left) break;
        if (na == 0) break;
        q = f[0];
        if (!legal(S.def, q)) break;
        line[n++] = q;
        put_v(q, S.def);
        played++;
        nd = fives_near(S.def, q, df, 2);
    }
    while (played--) undo_v();
    return n;
}

static int ensure_tt(void) {
    int ep = __atomic_load_n(&fv_epoch, __ATOMIC_RELAXED);
    if (!tts[tid]) {
        tts[tid] = calloc(TT_SIZE, sizeof(Ent));
        if (!tts[tid]) return 0;
        tt_epoch[tid] = ep;
    } else if (tt_epoch[tid] != ep) {
        memset(tts[tid], 0, sizeof(Ent) * TT_SIZE);
        tt_epoch[tid] = ep;
    }
    tt = tts[tid];
    return 1;
}

static void setup(Board *b, int attacker, int useset) {
    if (++S.gen == 0) {
        memset(S.st, 0, sizeof S.st);
        memset(S.fs, 0, sizeof S.fs);
        S.gen = 1;
    }
    S.b = b;
    S.att = attacker;
    S.def = opp_color(attacker);
    S.restricted = rule_has_forbidden(&b->rule);
    S.nodes = 0;
    S.aborted = 0;
    S.root_best = -1;
    S.hash = b->hash;
    S.nov = 0;
    S.nf = 0;
    S.useset = useset;
    memcpy(S.cell, b->cell, sizeof S.cell);
}

int fvcf_solve(Board *b, int attacker, int max_depth, long max_nodes, TssResult *res) {
    pthread_once(&z_once, z_init);
    if (!ensure_tt()) {
        if (res) memset(res, 0, sizeof *res), res->move = -1;
        return 0;
    }
    setup(b, attacker, 1);
    S.max_nodes = max_nodes;

    // 根: 五の点と四の点 (盤の集計で、探す点を全部見つけたら走査をやめる)
    const int16_t *ca = b->p4cnt[S.att], *cd = b->p4cnt[S.def];
    int want_af = ca[P4_FIVE] > 0, want_df = cd[P4_FIVE] < 2 ? cd[P4_FIVE] : 2;
    int want = ca[P4_BLOCK4] + ca[P4_BLOCK4_PLUS] + ca[P4_BLOCK4_FLEX3] + ca[P4_FLEX4];
    S.root_af = -1;
    S.root_nd = 0;
    S.root_df[0] = S.root_df[1] = -1;
    for (int y = 0; y < b->size && (want_af || want_df || want > 0); y++)
        for (int x = 0; x < b->size; x++) {
            int p = to_pos(x, y);
            if (b->cell[p] != EMPTY) continue;
            if (want_af && b->pat4[S.att][p] == P4_FIVE) { S.root_af = p; want_af = 0; }
            if (want_df && b->pat4[S.def][p] == P4_FIVE) { S.root_df[S.root_nd++] = p; want_df--; }
            if (want > 0 && is_four(b->pat4[S.att][p])) { set_add(p); want--; }
        }

    int win = a_node(max_depth);
    if (res) {
        res->win = win;
        res->move = win ? S.root_best : -1;
        res->nodes = S.nodes;
        res->aborted = S.aborted;
        res->nline = 0;
        if (win) {
            S.aborted = 0;
            res->nline = walk(S.root_best, res->line, TSS_MAX_LINE);
        }
    }
    return win;
}

// tz.c の replay と同じ (重ね盤の上で)。重ね盤は key だけを保ち、五の点は直前の手を通る線だけを見る:
//   - 根に攻め側の五の点があれば tz.c も最初に打ち切る。その後の攻め側の五の点は攻め側の直前の手を通る線にしかなく
//     (受け側が止めた後は残らない)、受け側の五の点は根のもの (攻め側の石では増えない) か受け側の直前の手を通る線のもの
//     (受け側が打つ前には残っていない) だけなので、tz.c の盤全体の数と同じになる。
//   - 2 本の線は 1 点でしか交わらないので、直前の手を通る線上の点はちょうど 1 回ずつ (方向・距離の順に) 数えられる
static inline void zadd(TzZone *z, int q) {
    if (q < 0 || q >= NCELL || S.cell[q] != EMPTY || S.zst[q] == S.zgen || z->n >= TZ_MAX_CELLS) return;
    S.zst[q] = S.zgen;
    z->cells[z->n++] = q;
}

int fvcf_replay(Board *b, int att, const int16_t *line, int n, TzZone *z) {
    pthread_once(&z_once, z_init);
    z->n = 0;
    z->first = n > 0 ? line[0] : -1;
    z->plies = 1;
    if (b->p4cnt[att][P4_FIVE]) return 0;   // tz.c: 根に攻め側の五の点、直前の攻めの手がないので 0
    setup(b, att, 0);
    if (++S.zgen == 0) {
        memset(S.zst, 0, sizeof S.zst);
        S.zgen = 1;
    }
    int def = S.def;
    int played = 0, ok = 0, i = 0;
    int nd = b->p4cnt[def][P4_FIVE];        // 受け側の五の点の数
    int df[8], ndf = -1;                    // 受け側の直前の手を通る線の五の点 (-1 = 根: 盤の集計を使う)
    for (;;) {
        if (i >= n) break;
        int a = line[i++];
        if (a < 0 || a >= NCELL || S.cell[a] != EMPTY) break;
        int a_is_d5 = 0;
        if (ndf < 0) a_is_d5 = b->pat4[def][a] == P4_FIVE;
        else for (int j = 0; j < ndf; j++) a_is_d5 |= df[j] == a;
        if (nd >= 2 || (nd == 1 && !a_is_d5)) break;
        if (!legal(S.att, a)) break;
        zadd(z, a);
        put_r(a, S.att);
        played++;
        if (nd - a_is_d5 > 0) break;         // 受け側の五の点が残る
        int af[8];
        int nf = line_fives(S.att, a, af, 8);
        if (nf == 0) break;
        if (nf >= 2) {
            for (int j = 0; j < nf; j++) zadd(z, af[j]);
            ok = 1;
            break;
        }
        int q = af[0];
        if (!legal(def, q)) { ok = 2; break; }
        zadd(z, q);
        for (int d = 0; d < 4; d++) {
            if (pat(def, q, d) < P_B3) continue;
            for (int k = -4; k <= 4; k++) zadd(z, q + k * DIR_OFFSET[d]);
        }
        if (i < n) i++;
        put_r(q, def);
        played++;
        ndf = line_fives(def, q, df, 8);
        nd = ndf;
    }
    z->plies = played + 1;
    while (played--) undo_r();
    return ok;
}
