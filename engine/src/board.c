#include "board.h"

#include "eval.h"

#include <string.h>

static uint64_t zobrist[3][NCELL];
static const int PIDX_W[4] = { P_COUNT * P_COUNT * P_COUNT, P_COUNT * P_COUNT, P_COUNT, 1 };   // pattern_index の各方向の重み
static uint64_t zobrist_pass;

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
    zobrist_pass = splitmix64(&s);
}

void board_pass(Board *b) {
    b->passes ^= 1;
    b->hash ^= zobrist_pass;
}

// 空点 q の集計 (vsum, p4cnt) への寄与を足す (sign=+1) / 引く (sign=-1)
static inline void account_cell(Board *b, int q, int sign) {
    for (int c = BLACK; c <= WHITE; c++) {
        b->vsum[c] += sign * g_cell_value[c][b->pidx[c][q]];
        b->p4cnt[c][b->pat4[c][q]] += (int16_t)sign;
    }
}

static void refresh_cell(Board *b, int q) {
    int empty = b->cell[q] == EMPTY;
    if (empty) account_cell(b, q, -1);
    for (int c = BLACK; c <= WHITE; c++) {
        uint8_t *pt = b->pat[c][q];
        for (int d = 0; d < 4; d++) pt[d] = g_pattern_table[c][board_key(b, q, d)];
        b->pidx[c][q] = (uint16_t)pattern_index(pt);
        b->pat4[c][q] = g_p4_table[b->pidx[c][q]];
    }
    if (empty) account_cell(b, q, +1);
}

// q の方向 d の線パターンだけを作り直す (石の出入りで変わるのはその方向だけ)。変わらなければ何もしない
// 石のあるマスのパターンは使わないので更新しない (石を取るときに refresh_cell で作り直す)
static inline void refresh_dir(Board *b, int q, int d, uint32_t key) {
    uint8_t np2 = g_pattern2[key];
    uint8_t *pb = b->pat[BLACK][q], *pw = b->pat[WHITE][q];
    uint8_t nb = np2 & 15, nw = np2 >> 4;
    if (nb == pb[d] && nw == pw[d]) return;
    BoardUndo *u = &b->undo[b->nundo++];
    u->q = (uint16_t)q;
    u->d = (uint8_t)d;
    u->pb = pb[d];
    u->pw = pw[d];
    u->p4b = b->pat4[BLACK][q];
    u->p4w = b->pat4[WHITE][q];
    u->ib = b->pidx[BLACK][q];
    u->iw = b->pidx[WHITE][q];
    if (nb != pb[d]) {
        int oi = b->pidx[BLACK][q], idx = oi + (nb - pb[d]) * PIDX_W[d];
        b->vsum[BLACK] += g_cell_value[BLACK][idx] - g_cell_value[BLACK][oi];
        b->p4cnt[BLACK][b->pat4[BLACK][q]]--;
        pb[d] = nb;
        b->pidx[BLACK][q] = (uint16_t)idx;
        b->pat4[BLACK][q] = g_p4_table[idx];
        b->p4cnt[BLACK][b->pat4[BLACK][q]]++;
    }
    if (nw != pw[d]) {
        int oi = b->pidx[WHITE][q], idx = oi + (nw - pw[d]) * PIDX_W[d];
        b->vsum[WHITE] += g_cell_value[WHITE][idx] - g_cell_value[WHITE][oi];
        b->p4cnt[WHITE][b->pat4[WHITE][q]]--;
        pw[d] = nw;
        b->pidx[WHITE][q] = (uint16_t)idx;
        b->pat4[WHITE][q] = g_p4_table[idx];
        b->p4cnt[WHITE][b->pat4[WHITE][q]]++;
    }
}

// 周囲 5x5 の石の数を増減する。1 行の 5 マスを 8 バイトの 1 回の加減でまとめる (各マスは 25 以下なので桁上がりしない)
static inline void add_near(Board *b, int p, int delta) {
    const uint64_t five = 0x0101010101ull;
    for (int dy = -2; dy <= 2; dy++) {
        uint8_t *r = &b->near[p + dy * STRIDE - 2];
        uint64_t w;
        memcpy(&w, r, 8);
        w = delta > 0 ? w + five : w - five;
        memcpy(r, &w, 8);
    }
}

uint8_t g_line_id[4][NCELL], g_line_pos[4][NCELL];

static void line_tables_init(void) {
    static int done;
    if (done) return;
    for (int p = 0; p < NCELL; p++) {
        int gx = p % STRIDE, gy = p / STRIDE;
        g_line_id[0][p] = (uint8_t)gy, g_line_pos[0][p] = (uint8_t)gx;
        g_line_id[1][p] = (uint8_t)gx, g_line_pos[1][p] = (uint8_t)gy;
        g_line_id[2][p] = (uint8_t)(gx - gy + STRIDE - 1), g_line_pos[2][p] = (uint8_t)gy;
        g_line_id[3][p] = (uint8_t)(gx + gy), g_line_pos[3][p] = (uint8_t)gy;
    }
    done = 1;
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
    b->bbox[0] = b->bbox[2] = (int8_t)size;
    b->bbox[1] = b->bbox[3] = -1;
    for (int p = 0; p < NCELL; p++) b->cell[p] = WALL;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) b->cell[to_pos(x, y)] = EMPTY;
    line_tables_init();
    for (int p = 0; p < NCELL; p++)
        for (int d = 0; d < 4; d++) b->line[d][g_line_id[d][p]] |= (uint64_t)b->cell[p] << (2 * g_line_pos[d][p]);
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int q = to_pos(x, y);
            b->cell[q] = WALL;      // 集計に入れずにパターンだけ作る
            refresh_cell(b, q);
            b->cell[q] = EMPTY;
            account_cell(b, q, +1);
        }
    }
}

// 色を指定して石を置く/取る (履歴は触らない)
static void put_stone(Board *b, int p, int color) {
    b->undo_mark[b->nmoves] = b->nundo;
    BoardSums *sv = &b->sums_before[b->nmoves];
    memcpy(sv->vsum, b->vsum, sizeof sv->vsum);
    memcpy(sv->p4cnt, b->p4cnt, sizeof sv->p4cnt);
    memcpy(sv->bbox, b->bbox, sizeof sv->bbox);
    {
        int x = pos_x(p), y = pos_y(p);
        uint32_t cols = ((0x1Fu << x) >> 2) & ((1u << b->size) - 1);
        for (int r = 0; r < 5; r++) {
            int yy = y - 2 + r;
            if (yy < 0 || yy >= b->size) continue;
            sv->nearmask[r] = b->nearmask[yy];
            b->nearmask[yy] |= cols;
        }
        b->stonemask[y] |= 1u << x;
        if (x < b->bbox[0]) b->bbox[0] = (int8_t)x;
        if (x > b->bbox[1]) b->bbox[1] = (int8_t)x;
        if (y < b->bbox[2]) b->bbox[2] = (int8_t)y;
        if (y > b->bbox[3]) b->bbox[3] = (int8_t)y;
    }
    account_cell(b, p, -1);
    add_near(b, p, +1);
    b->cell[p] = (uint8_t)color;
    b->hash ^= zobrist[color][p];
    for (int d = 0; d < 4; d++) {
        int off = DIR_OFFSET[d], lp = g_line_pos[d][p];
        uint64_t *lw = &b->line[d][g_line_id[d][p]];
        uint64_t w = *lw | (uint64_t)color << (2 * lp);
        *lw = w;
        // 石のあるマス・盤外のパターンは使わないので更新しない (石を取るときに戻す)
        for (int k = 1; k <= 5; k++) {
            int q = p + k * off;
            if (b->cell[q] == EMPTY) refresh_dir(b, q, d, line_key(w, lp + k));
            q = p - k * off;
            if (b->cell[q] == EMPTY) refresh_dir(b, q, d, line_key(w, lp - k));
        }
    }
}

void board_probe_sums(const Board *b, int p, int color, BoardSums *out) {
    memcpy(out->vsum, b->vsum, sizeof out->vsum);
    memcpy(out->p4cnt, b->p4cnt, sizeof out->p4cnt);
    for (int c = BLACK; c <= WHITE; c++) {
        out->vsum[c] -= g_cell_value[c][b->pidx[c][p]];
        out->p4cnt[c][b->pat4[c][p]]--;
    }
    for (int d = 0; d < 4; d++) {
        int off = DIR_OFFSET[d], w = PIDX_W[d], lp = g_line_pos[d][p];
        uint64_t lw = b->line[d][g_line_id[d][p]] | (uint64_t)color << (2 * lp);   // 置いた後の直線
        for (int k = -5; k <= 5; k++) {
            if (!k) continue;
            int q = p + k * off;
            if (b->cell[q] != EMPTY) continue;
            uint8_t np2 = g_pattern2[line_key(lw, lp + k)];
            const uint8_t *pb = b->pat[BLACK][q], *pw = b->pat[WHITE][q];
            int nb = np2 & 15, nw = np2 >> 4;
            if (nb != pb[d]) {
                int oi = b->pidx[BLACK][q], idx = oi + (nb - pb[d]) * w;
                out->vsum[BLACK] += g_cell_value[BLACK][idx] - g_cell_value[BLACK][oi];
                out->p4cnt[BLACK][b->pat4[BLACK][q]]--;
                out->p4cnt[BLACK][g_p4_table[idx]]++;
            }
            if (nw != pw[d]) {
                int oi = b->pidx[WHITE][q], idx = oi + (nw - pw[d]) * w;
                out->vsum[WHITE] += g_cell_value[WHITE][idx] - g_cell_value[WHITE][oi];
                out->p4cnt[WHITE][b->pat4[WHITE][q]]--;
                out->p4cnt[WHITE][g_p4_table[idx]]++;
            }
        }
    }
}

// 直前の put_stone を取り消す (着手は必ず後から順に戻すので、記録した前の値に戻せばよい。
// p 自身のパターンは石がある間は更新していないので、置く前のまま)
static void remove_stone(Board *b, int p) {
    int color = b->cell[p];
    b->cell[p] = EMPTY;
    add_near(b, p, -1);
    b->hash ^= zobrist[color][p];
    for (int d = 0; d < 4; d++) b->line[d][g_line_id[d][p]] &= ~(3ull << (2 * g_line_pos[d][p]));
    int mark = b->undo_mark[b->nmoves];
    for (int i = b->nundo - 1; i >= mark; i--) {
        const BoardUndo *u = &b->undo[i];
        b->pat[BLACK][u->q][u->d] = u->pb;
        b->pat[WHITE][u->q][u->d] = u->pw;
        b->pidx[BLACK][u->q] = u->ib;
        b->pidx[WHITE][u->q] = u->iw;
        b->pat4[BLACK][u->q] = u->p4b;
        b->pat4[WHITE][u->q] = u->p4w;
    }
    b->nundo = mark;
    const BoardSums *sv = &b->sums_before[b->nmoves];
    memcpy(b->vsum, sv->vsum, sizeof sv->vsum);
    memcpy(b->p4cnt, sv->p4cnt, sizeof sv->p4cnt);
    memcpy(b->bbox, sv->bbox, sizeof b->bbox);
    int x = pos_x(p), y = pos_y(p);
    for (int r = 0; r < 5; r++) {
        int yy = y - 2 + r;
        if (yy >= 0 && yy < b->size) b->nearmask[yy] = sv->nearmask[r];
    }
    b->stonemask[y] &= ~(1u << x);
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
    b->nmoves++;   // 取り消しの記録の位置をずらす (手番・履歴は使わない)
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
    b->nmoves--;
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
                if (board_key(b, q, d) != compute_key(b, q, d)) return 0;
                if (b->cell[q] != EMPTY) continue;   // 石のあるマスのパターンは更新しない
                for (int c = BLACK; c <= WHITE; c++)
                    if (b->pat[c][q][d] != pattern_of(c, board_key(b, q, d))) return 0;
            }
            if (b->cell[q] != EMPTY) continue;
            for (int c = BLACK; c <= WHITE; c++)
                if (b->pidx[c][q] != pattern_index(b->pat[c][q])) return 0;
            for (int c = BLACK; c <= WHITE; c++)
                if (b->pat4[c][q] != combine_pattern4(b->pat[c][q])) return 0;
        }
    }
    int8_t bb[4] = { (int8_t)b->size, -1, (int8_t)b->size, -1 };
    for (int y = 0; y < b->size; y++)
        for (int x = 0; x < b->size; x++) {
            int c = b->cell[to_pos(x, y)];
            if (c != BLACK && c != WHITE) continue;
            if (x < bb[0]) bb[0] = (int8_t)x;
            if (x > bb[1]) bb[1] = (int8_t)x;
            if (y < bb[2]) bb[2] = (int8_t)y;
            if (y > bb[3]) bb[3] = (int8_t)y;
        }
    if (memcmp(bb, b->bbox, sizeof bb)) return 0;
    for (int y = 0; y < b->size; y++) {
        uint32_t nm = 0, st = 0;
        for (int x = 0; x < b->size; x++) {
            int q = to_pos(x, y);
            if (b->near[q]) nm |= 1u << x;
            if (b->cell[q] == BLACK || b->cell[q] == WHITE) st |= 1u << x;
        }
        if (nm != b->nearmask[y] || st != b->stonemask[y]) return 0;
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
