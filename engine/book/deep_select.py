#!/usr/bin/env python3
"""ブックの手を古典 Rapfi の長い読みで選び直す (NNUE は使わない)。

  python3 engine/book/deep_select.py "7,7 7,6 6,6" ["7,7 7,6 6,5 6,7 8,5" ...] [--n1 2000000] [--k 4] [--n2 30000000] [--n3 0]

各局面について、石の周り 2 マスの空点に打った後の局面を Rapfi で評価し (n1 ノード)、手番側から見て良い順に
上位 k 手を n2 ノード (n3 > 0 なら残った上位 2 手をさらに n3 ノード) で評価し直して、最善の手を出す。
詰みのない手は評価順、詰みのある手は長い詰みほど良いとする。出力: 局面<TAB>手<TAB>評価 (手番側から)<TAB>候補の一覧
"""
import argparse, os, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
LABEL = os.path.join(HERE, "..", "tests", "rapfi_label.py")


def label(positions, nodes, j):
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
        f.write("\n".join(positions) + "\n")
    out = subprocess.run([sys.executable, LABEL, "--nodes", str(nodes), "-j", str(j), f.name],
                         capture_output=True, text=True, check=True).stdout.splitlines()
    return [l.split()[0] if l.split() else "None" for l in out]


def mover_value(ev):
    """打った後の局面の評価 (相手から見た値) → 打った側から見た並べ替えの値 (大きいほど良い)"""
    if ev in ("None", "error"):
        return -10 ** 9
    if "M" in ev:
        n = int(ev.lstrip("+-M"))
        return -10 ** 6 + n if not ev.startswith("-") else 10 ** 6 - n   # 相手の詰み: 長いほど良い
    return -int(ev)


def candidates(pos):
    st = set(tuple(map(int, m.split(","))) for m in pos.split())
    c = set()
    for x, y in st:
        for dx in range(-2, 3):
            for dy in range(-2, 3):
                q = (x + dx, y + dy)
                if q not in st and 0 <= q[0] < 15 and 0 <= q[1] < 15:
                    c.add(q)
    return ["%d,%d" % q for q in sorted(c)]


ap = argparse.ArgumentParser()
ap.add_argument("positions", nargs="+")
ap.add_argument("--n1", type=int, default=2000000)
ap.add_argument("--k", type=int, default=4)
ap.add_argument("--n2", type=int, default=30000000)
ap.add_argument("--n3", type=int, default=0)
ap.add_argument("-j", type=int, default=4)
a = ap.parse_args()

for pos in a.positions:
    cand = candidates(pos)
    ev1 = label([pos + " " + m for m in cand], a.n1, a.j)
    ranked = sorted(zip(cand, ev1), key=lambda t: -mover_value(t[1]))[:a.k]
    ev2 = label([pos + " " + m for m, _ in ranked], a.n2, a.j)
    ranked = sorted(zip([m for m, _ in ranked], ev2), key=lambda t: -mover_value(t[1]))
    if a.n3 > 0:
        top = ranked[:2]
        ev3 = label([pos + " " + m for m, _ in top], a.n3, a.j)
        ranked = sorted(zip([m for m, _ in top], ev3), key=lambda t: -mover_value(t[1])) + ranked[2:]
    best, ev = ranked[0]
    me = ("-" + ev[1:] if ev.startswith("+") else "+" + ev.lstrip("+-")) if "M" in ev else str(-int(ev)) if ev.lstrip("+-").isdigit() else ev
    print("%s\t%s\t%s\t%s" % (pos, best, me, " ".join("%s=%s" % t for t in ranked)), flush=True)
