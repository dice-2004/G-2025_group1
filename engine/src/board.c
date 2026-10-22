#include "board.h"

#include "eval.h"

#include <string.h>

static uint64_t zobrist[3][NCELL];

static uint64_t splitmix64(uint64_t *s) {
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

void board_global_init(void) {
    uint64_t s = 20251022;
    for (int c = 0; c < 3; c++)
        for (int p = 0; p < NCELL; p++) zobrist[c][p] = splitmix64(&s);
}

// 空点 q の集計 (vsum, p4cnt) への寄与を足す (sign=+1) / 引く (sign=-1)
static inline void account_cell(Board *b, int q, int sign) {
    for (int c = BLACK; c <= WHITE; c++) {
        b->vsum[c] += sign * g_cell_value[c][pattern_index(b->pat[c][q])];
        b->p4cnt[c][b->pat4[c][q]] += (int16_t)sign;
    }
}

static void refresh_cell(Board *b, int q) {
    int empty = b->cell[q] == EMPTY;
    if (empty) account_cell(b, q, -1);
    for (int c = BLACK; c <= WHITE; c++) {
        uint8_t *pt = b->pat[c][q];
        for (int d = 0; d < 4; d++) pt[d] = g_pattern_table[c][b->key[q][d]];
        b->pat4[c][q] = g_p4_table[pattern_index(pt)];
    }
    if (empty) account_cell(b, q, +1);
}

// q の方向 d の線パターンだけを作り直す (石の出入りで変わるのはその方向だけ)。変わらなければ何もしない
static inline void refresh_dir(Board *b, int q, int d) {
    int empty = b->cell[q] == EMPTY;
    for (int c = BLACK; c <= WHITE; c++) {
        uint8_t np = g_pattern_table[c][b->key[q][d]];
        uint8_t *pt = b->pat[c][q];
        if (np == pt[d]) continue;
        if (empty) {
            b->vsum[c] -= g_cell_value[c][pattern_index(pt)];
            b->p4cnt[c][b->pat4[c][q]]--;
        }
        pt[d] = np;
        int idx = pattern_index(pt);
        b->pat4[c][q] = g_p4_table[idx];
        if (empty) {
            b->vsum[c] += g_cell_value[c][idx];
            b->p4cnt[c][b->pat4[c][q]]++;
        }
    }
}

static inline void add_near(Board *b, int p, int delta) {
    for (int dy = -2; dy <= 2; dy++)
        for (int dx = -2; dx <= 2; dx++) b->near[p + dy * STRIDE + dx] += (uint8_t)delta;
}

static uint32_t compute_key(const Board *b, int q, int d) {
    uint32_t key = 0;
    for (int j = -5; j <= 5; j++) {
        if (j == 0) continue;
        int r = q + j * DIR_OFFSET[d];
        int v = (r >= 0 && r < NCELL) ? b->cell[r] : WALL;
        key |= (uint32_t)v << (2 * key_slot(j));
    }
    return key;
}

void board_init(Board *b, int size, const Rule *rule) {
    memset(b, 0, sizeof *b);
    b->size = size;
    b->rule = *rule;
    for (int p = 0; p < NCELL; p++) b->cell[p] = WALL;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) b->cell[to_pos(x, y)] = EMPTY;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int q = to_pos(x, y);
            for (int d = 0; d < 4; d++) b->key[q][d] = compute_key(b, q, d);
            b->cell[q] = WALL;      // 集計に入れずにパターンだけ作る
            refresh_cell(b, q);
            b->cell[q] = EMPTY;
            account_cell(b, q, +1);
        }
    }
}

// 色を指定して石を置く/取る (履歴は触らない)
static void put_stone(Board *b, int p, int color) {
    account_cell(b, p, -1);
    add_near(b, p, +1);
    b->cell[p] = (uint8_t)color;
    b->hash ^= zobrist[color][p];
    for (int d = 0; d < 4; d++) {
        int off = DIR_OFFSET[d];
        for (int k = 1; k <= 5; k++) {
            int q = p + k * off;   // q から見て p は -k
            if (b->cell[q] != WALL) {   // (盤外は番兵で埋まっているので、壁の先も壁)
                b->key[q][d] |= (uint32_t)color << (2 * key_slot(-k));
                refresh_dir(b, q, d);
            }
            q = p - k * off;       // q から見て p は +k
            if (b->cell[q] != WALL) {
                b->key[q][d] |= (uint32_t)color << (2 * key_slot(k));
                refresh_dir(b, q, d);
            }
        }
    }
}

static void remove_stone(Board *b, int p) {
    int color = b->cell[p];
    b->cell[p] = EMPTY;
    account_cell(b, p, +1);
    add_near(b, p, -1);
    b->hash ^= zobrist[color][p];
    for (int d = 0; d < 4; d++) {
        int off = DIR_OFFSET[d];
        for (int k = 1; k <= 5; k++) {
            int q = p + k * off;
            if (b->cell[q] != WALL) {
                b->key[q][d] &= ~(3u << (2 * key_slot(-k)));
                refresh_dir(b, q, d);
            }
            q = p - k * off;
            if (b->cell[q] != WALL) {
                b->key[q][d] &= ~(3u << (2 * key_slot(k)));
                refresh_dir(b, q, d);
            }
        }
    }
}

void board_move(Board *b, int pos) {
    put_stone(b, pos, board_side(b));
    b->moves[b->nmoves++] = pos;
}

// 色を指定して置く (VCF/VCT のパス = 相手番の攻めの検討用)。board_undo で戻せる
void board_move_color(Board *b, int pos, int color) {
    put_stone(b, pos, color);
    b->moves[b->nmoves++] = pos;
}

void board_undo(Board *b) {
    int pos = b->moves[--b->nmoves];
    remove_stone(b, pos);
}

// 黒石が p に置かれていない状態で、p が禁じ手か (再帰で偽の三を除く)
static int forbidden_rec(Board *b, int p, int depth) {
    const Rule *r = &b->rule;
    const uint8_t *pt = b->pat[BLACK][p];
    int f3dirs[4], nf3 = 0, fours = 0;
    for (int d = 0; d < 4; d++)
        if (pt[d] == P_F5) return 0;   // 五は禁じ手より優先
    for (int d = 0; d < 4; d++) {
        switch (pt[d]) {
        case P_OL: if (r->forbid_overline) return 1; break;
        case P_B4: case P_F4: fours += 1; break;
        case P_D4: fours += 2; break;
        case P_F3: f3dirs[nf3++] = d; break;
        default: break;
        }
    }
    if (r->forbid_44 && fours >= 2) return 1;
    if (!r->forbid_33 || nf3 < 2) return 0;
    if (depth > 8) return 0;

    // 活三の候補が 2 方向以上: 実際に「禁じ手でない手で達四にできるか」を確かめる
    put_stone(b, p, BLACK);
    int real = 0;
    for (int i = 0; i < nf3 && real < 2; i++) {
        int d = f3dirs[i], off = DIR_OFFSET[d];
        int ok = 0;
        for (int k = -4; k <= 4 && !ok; k++) {
            if (k == 0) continue;
            int q = p + k * off;
            if (b->cell[q] != EMPTY || b->pat[BLACK][q][d] != P_F4) continue;
            // q と p の間が全部黒 (= できる達四が p を含む)
            int step = k > 0 ? -1 : 1, contiguous = 1;
            for (int m = k + step; m != 0; m += step)
                if (b->cell[p + m * off] != BLACK) { contiguous = 0; break; }
            if (!contiguous) continue;
            if (!forbidden_rec(b, q, depth + 1)) ok = 1;
        }
        real += ok;
    }
    remove_stone(b, p);
    return real >= 2;
}

int board_is_forbidden(Board *b, int pos) {
    if (!rule_has_forbidden(&b->rule) || b->cell[pos] != EMPTY) return 0;
    return forbidden_rec(b, pos, 0);
}

int board_last_move_wins(const Board *b) {
    if (b->nmoves == 0) return 0;
    int p = b->moves[b->nmoves - 1];
    int color = b->cell[p];
    int exact = (color == BLACK) ? b->rule.black_exact_five : b->rule.white_exact_five;
    for (int d = 0; d < 4; d++) {
        int off = DIR_OFFSET[d], n = 1;
        for (int q = p + off; b->cell[q] == color; q += off) n++;
        for (int q = p - off; b->cell[q] == color; q -= off) n++;
        if (exact ? n == 5 : n >= 5) return color;
    }
    if (b->nmoves >= b->size * b->size) return 3;
    return 0;
}

int board_verify(const Board *b) {
    for (int y = 0; y < b->size; y++) {
        for (int x = 0; x < b->size; x++) {
            int q = to_pos(x, y);
            for (int d = 0; d < 4; d++) {
                if (b->key[q][d] != compute_key(b, q, d)) return 0;
                for (int c = BLACK; c <= WHITE; c++)
                    if (b->pat[c][q][d] != pattern_of(c, b->key[q][d])) return 0;
            }
            for (int c = BLACK; c <= WHITE; c++)
                if (b->pat4[c][q] != combine_pattern4(b->pat[c][q])) return 0;
        }
    }
    uint64_t h = 0;
    for (int p = 0; p < NCELL; p++)
        if (b->cell[p] == BLACK || b->cell[p] == WHITE) h ^= zobrist[b->cell[p]][p];
    if (h != b->hash) return 0;
    // 空点の集計と近傍の石数
    int32_t vsum[3] = {0};
    int16_t cnt[3][P4_COUNT] = {{0}};
    for (int y = 0; y < b->size; y++) {
        for (int x = 0; x < b->size; x++) {
            int q = to_pos(x, y), n = 0;
            for (int dy = -2; dy <= 2; dy++)
                for (int dx = -2; dx <= 2; dx++) {
                    int c = b->cell[q + dy * STRIDE + dx];
                    n += (c == BLACK || c == WHITE);
                }
            if (b->near[q] != n) return 0;
            if (b->cell[q] != EMPTY) continue;
            for (int c = BLACK; c <= WHITE; c++) {
                vsum[c] += g_cell_value[c][pattern_index(b->pat[c][q])];
                cnt[c][b->pat4[c][q]]++;
            }
        }
    }
    for (int c = BLACK; c <= WHITE; c++) {
        if (vsum[c] != b->vsum[c]) return 0;
        for (int k = 0; k < P4_COUNT; k++)
            if (cnt[c][k] != b->p4cnt[c][k]) return 0;
    }
    return 1;
}

void board_print(const Board *b, FILE *fp) {
    fprintf(fp, "   ");
    for (int x = 0; x < b->size; x++) fprintf(fp, "%2d", x % 100);
    fprintf(fp, "\n");
    int last = b->nmoves ? b->moves[b->nmoves - 1] : -1;
    for (int y = 0; y < b->size; y++) {
        fprintf(fp, "%2d ", y);
        for (int x = 0; x < b->size; x++) {
            int p = to_pos(x, y), c = b->cell[p];
            char ch = c == BLACK ? 'X' : c == WHITE ? 'O' : '.';
            if (p == last) ch = (char)(ch + ('a' - 'A'));
            fprintf(fp, " %c", ch);
        }
        fprintf(fp, "\n");
    }
}
