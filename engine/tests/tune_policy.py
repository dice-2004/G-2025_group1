#!/usr/bin/env python3
"""探索の手の並べ方 (policy) を Rapfi の最善手に合わせる。

  python3 engine/tests/tune_policy.py [--cache engine/cache/rapfi_labels.tsv] [--l2 1e-4] [--iters 300] [--out src/policy_data.c]

engine/cache/rapfi_labels.tsv (tune_eval.py --rapfi が作る「局面<TAB>Rapfi の評価 Rapfi の手」) の各局面で、
候補手 (石の周り 2 マス) の点を
    今の点 (攻め + 守り) * k + 自分の 4 方向の線パターンの点の和 + 自分の Pattern4 の点 + 相手の (同) + 直前の相手の手との距離の点
    + 自分の前の手との距離の点
(パターンの点は手番の色ごと、計 115 個の重み) とし、softmax で Rapfi の手の確率を最大にする (L2 つき)。
手で決めた特徴の線形和の重みを合わせるだけで、ニューラルネットではない。
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
row, color, cme, cop, dl, dm, y, base, p4me, p4op = D.T
nrow = row.max() + 1
print("candidates: %d, rows %d" % (len(row), nrow), file=sys.stderr)
PC, P4C = 12, 13   # 線パターンの種類 (pattern.h P_COUNT)、Pattern4 の種類 (P4_COUNT)


def digits(sp):
    return [sp // (PC ** 3), sp // (PC ** 2) % PC, sp // PC % PC, sp % PC]


# 特徴: 手番の色ごとに 自分の線パターン (4 方向の和) / 自分の Pattern4 / 相手の線パターン / 相手の Pattern4、
# 直前の相手の手との距離、自分の前の手との距離。どれも重み 1 の添字の列で表す
cs = color - 1
OFF_LME, OFF_CME = 0, 2 * PC
OFF_LOP, OFF_COP = OFF_CME + 2 * P4C, OFF_CME + 2 * P4C + 2 * PC
OFF_DL = OFF_COP + 2 * P4C
OFF_DM = OFF_DL + 7
IB = OFF_DM + 7
P = IB + 1
cols = [OFF_LME + cs * PC + d for d in digits(cme)] + [OFF_CME + cs * P4C + p4me] + \
       [OFF_LOP + cs * PC + d for d in digits(cop)] + [OFF_COP + cs * P4C + p4op] + [OFF_DL + dl, OFF_DM + dm]
bf = base.astype(float) / 100.0
mask = np.ones(P)
rng = np.random.default_rng(1)
val_rows = rng.random(nrow) < 0.1
is_val = val_rows[row]


def scores(w):
    t = w[IB] * bf
    for c in cols:
        t = t + w[c]
    return t


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
    grad = np.zeros(P)
    for c in cols:
        grad += np.bincount(c, weights=g, minlength=P)
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

    def arr(off, n):
        return "{%s}" % ", ".join(str(int(x)) for x in W[off:off + n])

    with open(a.out, "w") as f:
        f.write("// tune_policy.py が作る手の並べ方の重み (Rapfi の最善手に合わせた、線パターン・Pattern4・直前の手との距離の点)。\n")
        f.write("// 手で編集しない。1 logit = %d\n" % a.scale)
        f.write('#include "policy_data.h"\n\n')
        f.write("// [手番の色 黒=0 白=1][線パターン]\n")
        f.write("const int g_policy_line_me[2][12] = {%s, %s};\n" % (arr(OFF_LME, PC), arr(OFF_LME + PC, PC)))
        f.write("const int g_policy_line_op[2][12] = {%s, %s};\n" % (arr(OFF_LOP, PC), arr(OFF_LOP + PC, PC)))
        f.write("// [手番の色][Pattern4]\n")
        f.write("const int g_policy_p4_me[2][13] = {%s, %s};\n" % (arr(OFF_CME, P4C), arr(OFF_CME + P4C, P4C)))
        f.write("const int g_policy_p4_op[2][13] = {%s, %s};\n" % (arr(OFF_COP, P4C), arr(OFF_COP + P4C, P4C)))
        f.write("const int g_policy_dist_last[7] = %s;\n" % arr(OFF_DL, 7))
        f.write("const int g_policy_dist_mine[7] = %s;\n" % arr(OFF_DM, 7))
        f.write("const int g_policy_base_scale = %d;   // 今の候補手の点 1 あたり (/100)\n" % int(round(w[IB] * a.scale)))
    print("wrote %s (%d weights)" % (a.out, P), file=sys.stderr)
