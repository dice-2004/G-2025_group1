// 反証の圏 (refutation zone)。
// 本探索で手番側の手 m が alpha を超えなかった (相手に反証された) とき、反証の読み筋 (置換表の最善手の連なり) のマスを覚える。
// 五目並べは石が増えて損をしない (単調性) ので、m への反証は「パス」への反証でもあり、その読み筋に関わらない遠くの手 q も
// 同じ読み筋で反証されるはず、と考える。脅威圏 (tz.c) の考え方を、詰みでない評価値の読み筋に広げたもの (健全ではない)。
// 探索 (search.c) は、全部の手が alpha を超えない見込みの局面で、圏の外の静かな手を深く縮める / 読まない。
#ifndef G2_RZ_H
#define G2_RZ_H

#include "types.h"

#define RZ_MAX_PLY 128
#define RZ_CELLS 64

typedef struct {
    int n;
    int16_t cells[RZ_CELLS];
} RzZone;

extern _Thread_local RzZone g_rz[RZ_MAX_PLY];

static inline void rz_reset(int ply) { if (ply < RZ_MAX_PLY) g_rz[ply].n = 0; }

static inline void rz_add(int ply, int q) {
    if (ply >= RZ_MAX_PLY) return;
    RzZone *z = &g_rz[ply];
    for (int i = 0; i < z->n; i++)
        if (z->cells[i] == q) return;
    if (z->n < RZ_CELLS) z->cells[z->n++] = (int16_t)q;
}

static inline int rz_count(int ply) { return ply < RZ_MAX_PLY ? g_rz[ply].n : 0; }

// q が圏に入るか: 読み筋のマスから Chebyshev 距離 rad 以内、または (lines なら) 同じ線の上で 4 マス以内
int rz_relevant(int ply, int q, int rad, int lines);

#endif
