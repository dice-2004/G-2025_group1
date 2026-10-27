"""開局ブックの局面キー (engine/src/book.c と同じ計算)。

15 路の盤の中心 (7,7) を原点に 8 通りの対称変換 (回転・鏡映) をかけ、
石の Zobrist 風ハッシュ (固定の splitmix64 表) が最小になる変換を「正規形」とする。
"""

SIZE = 15
C = SIZE // 2
MASK = (1 << 64) - 1


def splitmix64(z):
    z = (z + 0x9E3779B97F4A7C15) & MASK
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & MASK
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & MASK
    return z ^ (z >> 31)


# Z[color][y][x]、color 1=黒 2=白。idx = ((color-1)*15 + y)*15 + x
Z = {c: [[splitmix64(((c - 1) * SIZE + y) * SIZE + x) for x in range(SIZE)] for y in range(SIZE)] for c in (1, 2)}


def transform(t, x, y):
    """変換 t (0..7) を (x, y) にかける。bit2: x と y を入れ替え、bit0: x 反転、bit1: y 反転 (この順)。"""
    a, b = x - C, y - C
    if t & 4:
        a, b = b, a
    if t & 1:
        a = -a
    if t & 2:
        b = -b
    return a + C, b + C


def inverse(t, x, y):
    a, b = x - C, y - C
    if t & 1:
        a = -a
    if t & 2:
        b = -b
    if t & 4:
        a, b = b, a
    return a + C, b + C


def stones_of(moves):
    """着手列 [(x,y)] -> [(x,y,color)]。黒から交互。"""
    return [(x, y, 1 if i % 2 == 0 else 2) for i, (x, y) in enumerate(moves)]


def key_of(stones, t):
    h = 0
    for x, y, c in stones:
        tx, ty = transform(t, x, y)
        h ^= Z[c][ty][tx]
    return h


def canonical(moves):
    """着手列 -> (key, t)。key は 8 変換のハッシュの最小、t はそれを与える最初の変換。"""
    st = stones_of(moves)
    best = None
    for t in range(8):
        k = key_of(st, t)
        if best is None or k < best[0]:
            best = (k, t)
    return best


def canonical_moves(moves):
    """着手列を正規形の座標に変換し、黒・白それぞれ座標順に並べ直して交互に並べた列 (手順違いの同一局面は同じ列) と変換 t"""
    _, t = canonical(moves)
    tm = [transform(t, x, y) for x, y in moves]
    bl = sorted(tm[0::2], key=lambda m: (m[1], m[0]))
    wh = sorted(tm[1::2], key=lambda m: (m[1], m[0]))
    out = []
    for i in range(len(tm)):
        out.append(bl[i // 2] if i % 2 == 0 else wh[i // 2])
    return out, t
