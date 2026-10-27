#!/usr/bin/env python3
"""空点の値の表 (4 方向のパターンの組ごと) を Rapfi の評価に合わせて補正する。

  python3 engine/tests/tune_table.py [--cache engine/build/rapfi_labels.tsv] [--min-count 200] [--l2 0.3]

今の評価 (eval.c の線 + 組み合わせの和) はそのままにして、4 方向のパターンの組 (方向の順序は無視) と色ごとに
補正値 delta を足す。評価 = 今の評価 + Σ(手番側の空点の delta) − opp_scale/64 · Σ(相手側の空点の delta)。
sigmoid(評価 / K) を Rapfi の評価 (tune_eval.py --rapfi で付けたキャッシュ) に合わせ、delta には L2 正則化。
十分な回数現れる組だけを調整する。結果は src/eval_table.c に書き出す (--write)。

ニューラルネットではなく、手で決めたパターン分類ごとの点の表を合わせる軽い調整。
"""
import argparse, os, subprocess, zlib
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ENGINE = os.path.dirname(HERE)
NP = 12
RAPFI_K = 300.0
LINE_NAMES = ["DEAD", "OL", "B1", "F1", "B2", "F2", "B3", "F3", "B4", "F4", "D4", "F5"]

ap = argparse.ArgumentParser()
ap.add_argument("--cache", default=os.path.join(ENGINE, "build", "rapfi_labels.tsv"))
ap.add_argument("--rule", type=int, default=4)
ap.add_argument("--min-count", type=int, default=200, help="この局面数以上に現れる組だけ調整する")
ap.add_argument("--l2", type=float, default=1.0)
ap.add_argument("--iters", type=int, default=3000)
ap.add_argument("--lr", type=float, default=2.0)
ap.add_argument("--opp-scale", type=float, default=None, help="既定は eval.c の値")
ap.add_argument("--write", action="store_true", help="src/eval_table.c に書き出す")
a = ap.parse_args()

src = open(os.path.join(ENGINE, "src", "eval.c")).read()
opp_scale = a.opp_scale or int(src[src.index(".opp_scale = ") + 13:].split(",")[0]) / 64.0

keys = [l.split("\t")[0] for l in open(a.cache)]
out = subprocess.run([os.path.join(ENGINE, "build", "g2-test"), "msdump", str(a.rule)], stdin=open(a.cache),
                     capture_output=True, text=True, check=True).stdout.splitlines()
rows = []
for key, l in zip(keys, out):
    head, sm, so = l.split("|")
    lab, me, quiet, ev = head.split()
    if quiet != "1" or lab in ("None", "error"):
        continue
    if "M" in lab:
        t = 1.0 if lab.startswith("+") or lab[0] == "M" else 0.0
    else:
        t = 1 / (1 + np.exp(-int(lab) / RAPFI_K))
    parse = lambda s: [(int(x.split(":")[0]), int(x.split(":")[1])) for x in s.split()]
    rows.append((key, int(me), int(ev), t, parse(sm), parse(so)))
print("positions:", len(rows))

# 調整する列: (色, 組) で十分な局面に現れるもの
occ = {}
for _, me, _, _, sm, so in rows:
    op = 3 - me
    for c, lst in ((me, sm), (op, so)):
        for i, _ in lst:
            occ[(c, i)] = occ.get((c, i), 0) + 1
cols = sorted(k for k, n in occ.items() if n >= a.min_count)
col = {k: j for j, k in enumerate(cols)}
print("columns:", len(cols))
X = np.zeros((len(rows), len(cols)), dtype=np.float32)
for r, (_, me, _, _, sm, so) in enumerate(rows):
    for i, n in sm:
        j = col.get((me, i))
        if j is not None:
            X[r, j] += n
    for i, n in so:
        j = col.get((3 - me, i))
        if j is not None:
            X[r, j] -= opp_scale * n
ev = np.array([r[2] for r in rows], dtype=float)
t = np.array([r[3] for r in rows])

# 対局 (最初の 6 手が同じ) 単位で 8:2 に分ける
grp = np.array([zlib.crc32(" ".join(r[0].split()[:6]).encode()) % 5 for r in rows])
vm = grp == 0


def loss(d, m, K):
    e = ev[m] + X[m] @ d
    p = 1 / (1 + np.exp(-e / K))
    return -np.mean(t[m] * np.log(p + 1e-9) + (1 - t[m]) * np.log(1 - p + 1e-9)), p


Ks = [100, 150, 200, 300, 400, 600, 800, 1200, 1600, 2400, 3200]
K = min(Ks, key=lambda k: loss(np.zeros(len(cols)), ~vm, k)[0])
d = np.zeros(len(cols))
print("K = %d, loss(current) train %.5f val %.5f" % (K, loss(d, ~vm, K)[0], loss(d, vm, K)[0]))
m1 = np.zeros_like(d); v1 = np.zeros_like(d)
Xt = X[~vm]
for it in range(1, a.iters + 1):
    l, p = loss(d, ~vm, K)
    g = Xt.T @ ((p - t[~vm]) / K / len(p)) + 2 * a.l2 * d / 1e7
    m1 = 0.9 * m1 + 0.1 * g; v1 = 0.999 * v1 + 0.001 * g * g
    d -= a.lr * (m1 / (1 - 0.9 ** it)) / (np.sqrt(v1 / (1 - 0.999 ** it)) + 1e-12)
    if it == 1:
        print("grad |g| mean %.3g max %.3g" % (np.abs(g).mean(), np.abs(g).max()))
    if it % 500 == 0:
        print("iter %d train %.5f val %.5f |d| %.1f" % (it, l, loss(d, vm, K)[0], np.abs(d).mean()))

di = np.round(d).astype(int)


def name(i):
    pats = [i // NP ** 3, i // NP ** 2 % NP, i // NP % NP, i % NP]
    return "+".join(LINE_NAMES[p] for p in pats if p) or "DEAD"


order = np.argsort(-np.abs(di))
for j in order[:25]:
    c, i = cols[j]
    print("%s %-16s n=%5d delta %+d" % ("黒" if c == 1 else "白", name(i), occ[(c, i)], di[j]))

if a.write:
    path = os.path.join(ENGINE, "src", "eval_table.c")
    ent = [(c, i, int(x)) for (c, i), x in zip(cols, di) if x]
    with open(path, "w") as f:
        f.write("// tests/tune_table.py で作った空点の値の補正 (色, 4 方向のパターンを小さい順に並べた組, 補正値)。\n")
        f.write("// Rapfi (古典評価) の評価 %d 局面に合わせた。連珠のみ\n" % len(rows))
        f.write('#include "eval.h"\n\nconst EvalTableEntry g_eval_table[] = {\n')
        for c, i, x in ent:
            f.write("    {%d, %d, %d},  // %s\n" % (c, i, x, name(i)))
        f.write("};\nconst int g_eval_table_count = %d;\nconst int g_eval_table_rule = %d;\n" % (len(ent), a.rule))
    print("wrote", path, len(ent), "entries")
