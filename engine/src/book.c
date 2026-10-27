// 開局ブックの表引き。キーの計算は engine/book/bookkey.py と同じ
#include "book.h"

#include "book_data.h"

#define BSIZE 15
#define BC (BSIZE / 2)

static int g_enabled = 1;
static uint64_t g_z[2][BSIZE][BSIZE];   // [color-1][y][x]
static int g_z_ready = 0;

void book_set_enabled(int on) { g_enabled = on; }
int book_enabled(void) { return g_enabled; }

static uint64_t mix64(uint64_t z) {
    z += 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static void init_z(void) {
    for (int c = 0; c < 2; c++)
        for (int y = 0; y < BSIZE; y++)
            for (int x = 0; x < BSIZE; x++) g_z[c][y][x] = mix64((uint64_t)((c * BSIZE + y) * BSIZE + x));
    g_z_ready = 1;
}

// 変換 t: bit2 で x,y 入れ替え、bit0 で x 反転、bit1 で y 反転 (この順)。中心は (7,7)
static void transform(int t, int x, int y, int *ox, int *oy) {
    int a = x - BC, b = y - BC;
    if (t & 4) { int s = a; a = b; b = s; }
    if (t & 1) a = -a;
    if (t & 2) b = -b;
    *ox = a + BC;
    *oy = b + BC;
}

static void inverse(int t, int x, int y, int *ox, int *oy) {
    int a = x - BC, b = y - BC;
    if (t & 1) a = -a;
    if (t & 2) b = -b;
    if (t & 4) { int s = a; a = b; b = s; }
    *ox = a + BC;
    *oy = b + BC;
}

uint64_t book_key(const Board *b, int *transform_out) {
    if (!g_z_ready) init_z();
    uint64_t h[8] = {0};
    for (int y = 0; y < BSIZE; y++) {
        for (int x = 0; x < BSIZE; x++) {
            int c = b->cell[to_pos(x, y)];
            if (c != BLACK && c != WHITE) continue;
            for (int t = 0; t < 8; t++) {
                int tx, ty;
                transform(t, x, y, &tx, &ty);
                h[t] ^= g_z[c - 1][ty][tx];
            }
        }
    }
    int best = 0;
    for (int t = 1; t < 8; t++)
        if (h[t] < h[best]) best = t;
    if (transform_out) *transform_out = best;
    return h[best];
}

static const BookEntry *lookup(const BookEntry *tab, int n, uint64_t key) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (tab[mid].key == key) return &tab[mid];
        if (tab[mid].key < key) lo = mid + 1;
        else hi = mid - 1;
    }
    return NULL;
}

int book_probe(Board *b, int *move) {
    if (!g_enabled || b->size != BSIZE || b->nmoves > g_book_max_stones) return 0;
    // 五や達四の点がある局面は探索に任せる
    for (int c = BLACK; c <= WHITE; c++)
        if (b->p4cnt[c][P4_FIVE] || b->p4cnt[c][P4_FLEX4]) return 0;
    // 石数が手番と合わない盤 (BOARD で置き石など) は使わない
    int nb = 0, nw = 0;
    for (int y = 0; y < BSIZE; y++)
        for (int x = 0; x < BSIZE; x++) {
            int c = b->cell[to_pos(x, y)];
            nb += c == BLACK;
            nw += c == WHITE;
        }
    if (nb + nw != b->nmoves || nb - nw != (b->nmoves & 1)) return 0;

    const BookEntry *tab = g_book_freestyle;
    int n = g_book_freestyle_n;
    if (rule_has_forbidden(&b->rule)) {
        tab = g_book_renju;
        n = g_book_renju_n;
    }
    int t;
    uint64_t key = book_key(b, &t);
    const BookEntry *e = lookup(tab, n, key);
    if (!e) return 0;
    int x, y;
    inverse(t, e->x, e->y, &x, &y);
    if (!board_on(b, x, y)) return 0;
    int p = to_pos(x, y);
    if (b->cell[p] != EMPTY) return 0;
    if (board_side(b) == BLACK && board_is_forbidden(b, p)) return 0;
    *move = p;
    return 1;
}
