#!/usr/bin/env python3
"""探索の手の並べ方 (policy) を Rapfi の最善手に合わせる。

  python3 engine/tests/tune_policy.py [--cache engine/cache/rapfi_labels.tsv] [--l2 1e-4] [--iters 300] [--out src/policy_data.c]

engine/cache/rapfi_labels.tsv (tune_eval.py --rapfi が作る「局面<TAB>Rapfi の評価 Rapfi の手」) の各局面で、
候補手 (石の周り 2 マス) の点を
    自分の 4 方向パターンの組 (順不同) の表 + 相手の組の表 + 直前の相手の手との距離 + 自分の前の手との距離
(表は手番の色ごと) とし、softmax で Rapfi の手の確率を最大にする (L2 つき)。学習するのは手で決めた
パターンの組ごとの数値の表 (線形) で、ニューラルネットではない。
いまの並べ方 (攻め + 守り * 52/64) との 1 番目・3 番目までの一致率を検証用の 1 割で比べ、表を C で書き出す。
"""
import argparse, os, subprocess, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ENGINE = os.path.join(HERE, "..")
ap = argparse.ArgumentParser()
ap.add_argument("--cache", default=os.path.join(ENGINE, "cache", "rapfi_labels.tsv"))
ap.add_argument("--l2", type=float, default=1e-4)
ap.add_argument("--iters", type=int, default=300)
ap.add_argument("--lr", type=float, default=0.05)
ap.add_argument("--scale", type=int, default=256, help="C の表の 1 (logit) あたりの整数")
ap.add_argument("--min-count", type=int, default=20, help="これより少ない組は 0 (学習しない)")
ap.add_argument("--out", default=None)
ap.add_argument("--rule", default="4")
a = ap.parse_args()

lines = []
for l in open(a.cache):
    k, v = l.rstrip("\n").split("\t")
    f = v.split()
    if len(f) < 2 or "," not in f[1] or f[0] in ("None", "error"):
        continue
    lines.append("%s\t%s" % (k, f[1]))
print("positions:", len(lines), file=sys.stderr)
out = subprocess.run([os.path.join(ENGINE, "build", "g2-test"), "polfeat", a.rule], input="\n".join(lines) + "\n",
                     capture_output=True, text=True, check=True).stdout
D = np.loadtxt(out.splitlines(), dtype=np.int64)
row, color, cme, cop, dl, dm, y, base = D.T
nrow = row.max() + 1
print("candidates: %d, rows %d" % (len(row), nrow), file=sys.stderr)

# パターンの組を詰めた番号に (色ごと・役ごと)
keys_me = color * 100000 + cme
keys_op = color * 100000 + cop
uk_me, im = np.unique(keys_me, return_inverse=True)
uk_op, io = np.unique(keys_op, return_inverse=True)
cnt_me = np.bincount(im)
cnt_op = np.bincount(io)
use_me = cnt_me >= a.min_count
use_op = cnt_op >= a.min_count
nme, nop = len(uk_me), len(uk_op)
P = nme + nop + 7 + 7 + 1   # 最後の 1 つは今の候補手の点 (攻め + 守り) の係数 (今の並べ方から始める)
IB = P - 1
bf = base.astype(float) / 100.0
idx_me, idx_op, idx_dl, idx_dm = im, nme + io, nme + nop + dl, nme + nop + 7 + dm
mask = np.ones(P)
mask[:nme] = use_me
mask[nme:nme + nop] = use_op

rng = np.random.default_rng(1)
val_rows = rng.random(nrow) < 0.1
is_val = val_rows[row]


def scores(w):
    return w[idx_me] + w[idx_op] + w[idx_dl] + w[idx_dm] + w[IB] * bf


def group_softmax(s):
    m = np.full(nrow, -np.inf)
    np.maximum.at(m, row, s)
    e = np.exp(s - m[row])
    z = np.bincount(row, weights=e, minlength=nrow)
    return e / z[row], np.log(z) + m


def accuracy(s, sel):
    # 正解の手より点の高い候補の数 (同点は半分) + 1 = 順位
    st = np.zeros(nrow)
    st[row[y == 1]] = s[y == 1]
    higher = np.bincount(row, weights=(s > st[row]).astype(float), minlength=nrow)
    ties = np.bincount(row, weights=(s == st[row]).astype(float), minlength=nrow) - 1
    rank = higher + ties / 2 + 1
    r = rank[sel]
    return (r <= 1).mean(), (r <= 3).mean(), r.mean()


vsel = val_rows & (np.bincount(row, weights=y, minlength=nrow) == 1)
tsel = ~val_rows & (np.bincount(row, weights=y, minlength=nrow) == 1)
b1, b3, br = accuracy(base.astype(float), vsel)
print("baseline (attack + defense*52/64): top1 %.3f top3 %.3f avg rank %.2f" % (b1, b3, br), file=sys.stderr)

w = np.zeros(P)
w[IB] = 1.0
mw, vw = np.zeros(P), np.zeros(P)
tr = ~is_val
for it in range(1, a.iters + 1):
    s = scores(w)
    p, lz = group_softmax(s)
    g = (p - y) * tr
    grad = (np.bincount(idx_me, weights=g, minlength=P) + np.bincount(idx_op, weights=g, minlength=P) +
            np.bincount(idx_dl, weights=g, minlength=P) + np.bincount(idx_dm, weights=g, minlength=P))
    grad[IB] = (g * bf).sum()
    reg = a.l2 * w
    reg[IB] = 0
    grad = grad / tsel.sum() + reg
    grad *= mask
    mw = 0.9 * mw + 0.1 * grad
    vw = 0.999 * vw + 0.001 * grad * grad
    w -= a.lr * (mw / (1 - 0.9 ** it)) / (np.sqrt(vw / (1 - 0.999 ** it)) + 1e-8)
    if it % 50 == 0 or it == a.iters:
        ll = -(np.log(np.maximum(p[(y == 1) & tr], 1e-12))).mean()
        t1, t3, tr_ = accuracy(s, vsel)
        print("iter %d train nll %.3f  val top1 %.3f top3 %.3f avg rank %.2f" % (it, ll, t1, t3, tr_), file=sys.stderr)

if a.out:
    W = np.round(w * a.scale).astype(int)
    with open(a.out, "w") as f:
        f.write("// tune_policy.py が作る手の並べ方の表 (Rapfi の最善手に合わせた、パターンの組ごとの点)。手で編集しない\n")
        f.write('#include "policy_data.h"\n\n')
        ent = []
        for i, k in enumerate(uk_me):
            if use_me[i] and W[i]:
                ent.append((int(k // 100000), 0, int(k % 100000), int(W[i])))
        for i, k in enumerate(uk_op):
            if use_op[i] and W[nme + i]:
                ent.append((int(k // 100000), 1, int(k % 100000), int(W[nme + i])))
        f.write("const PolicyEntry g_policy_entries[] = {\n")
        for e in ent:
            f.write("    {%d, %d, %d, %d},\n" % e)
        f.write("};\nconst int g_policy_count = %d;\n" % len(ent))
        f.write("const int g_policy_dist_last[7] = {%s};\n" % ", ".join(str(int(x)) for x in W[nme + nop:nme + nop + 7]))
        f.write("const int g_policy_dist_mine[7] = {%s};\n" % ", ".join(str(int(x)) for x in W[nme + nop + 7:nme + nop + 14]))
        f.write("const int g_policy_base_scale = %d;   // 今の候補手の点 1 あたり (/100)\n" % int(round(w[IB] * a.scale)))
    print("wrote %s (%d entries)" % (a.out, len(ent)), file=sys.stderr)
