// 反証の圏 (説明は rz.h)
#include "rz.h"

#include <stdlib.h>

_Thread_local RzZone g_rz[RZ_MAX_PLY];

int rz_relevant(int ply, int q, int rad, int lines) {
    if (ply >= RZ_MAX_PLY) return 1;
    const RzZone *z = &g_rz[ply];
    int qx = pos_x(q), qy = pos_y(q);
    for (int i = 0; i < z->n; i++) {
        int dx = abs(pos_x(z->cells[i]) - qx), dy = abs(pos_y(z->cells[i]) - qy);
        int d = dx > dy ? dx : dy;
        if (d <= rad) return 1;
        if (lines && d <= 4 && (dx == 0 || dy == 0 || dx == dy)) return 1;
    }
    return 0;
}
