#!/usr/bin/env python3
"""評価の重み (eval.c の line / combo / opp_scale / tempo) を棋譜の勝敗に合わせて軽く調整する (Texel 法)。

  python3 engine/tests/tune_eval.py bench/results/*/games.jsonl [--rule renju] [--iters 3000]

1. 棋譜 (games.jsonl) の静かな局面 (五・達四の点がない) から特徴量を g2-test features で作る
2. 評価 = w・f(手番側) − s・w・f(相手側) + tempo を sigmoid(評価 / K) にして勝敗との交差エントロピーを最小化
   (w = 線パターンの重み 12 個 + Pattern4 の加点 13 個、s = opp_scale/64)
3. 今の重みと調整後の重み、検証用データでの損失を表示し、INFO p_* の形と eval.c の初期化子の形で出す

ニューラルネットではなく、手で決めた評価の形の「閾値・重み」だけを合わせる軽い学習。
"""
import argparse, json, os, subprocess, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ENGINE = os.path.dirname(HERE)
NP, NP4 = 12, 13
RULE = {"renju": 4, "freestyle": 0, "standard": 1}
LINE_NAMES = ["DEAD", "OL", "B1", "F1", "B2", "F2", "B3", "F3", "B4", "F4", "D4", "F5"]
P4_NAMES = ["NONE", "F2", "B3", "F2x2", "B3+", "F3", "F3+", "F3x2", "B4", "B4+", "B4F3", "F4", "FIVE"]


def current_params():
    src = open(os.path.join(ENGINE, "src", "eval.c")).read()
    def arr(name):
        i = src.index("." + name + " = {")
        return [int(v) for v in src[src.index("{", i) + 1:src.index("}", i)].split(",")]
    def val(name):
        i = src.index("." + name + " = ")
        return int(src[i + len(name) + 4:src.index(",", i)])
    return np.array(arr("line") + arr("combo"), dtype=float), val("opp_scale") / 64.0, float(val("tempo"))


def load(paths, rule):
    lines = []
    for p in paths:
        for l in open(p):
            g = json.loads(l)
            if g.get("rule") != rule or g.get("winner") is None:
                continue
            w = g["winner"]
            res = 1 if w == "black" else -1 if w == "white" else 0
            mv = [tuple(m) for m in g["opening"]] + [tuple(m) for m in g["moves"]]
            # 禁じ手負け・時間切れで終わった局は最後の手が不自然なので、勝敗だけ使う
            lines.append("%d %s" % (res, " ".join("%d,%d" % m for m in mv)))
    out = subprocess.run([os.path.join(ENGINE, "build", "g2-test"), "features", str(RULE[rule])],
                         input="\n".join(lines) + "\n", capture_output=True, text=True, check=True).stdout
    a = np.array([[int(v) for v in r.split(",")] for r in out.split()], dtype=float)
    return a[:, 0], a[:, 3], a[:, 4:4 + NP + NP4], a[:, 4 + NP + NP4:]


def loss_grad(theta, fm, fo, y, K, l2):
    w, s, t = theta[:-2], theta[-2], theta[-1]
    em, eo = fm @ w, fo @ w
    e = em - s * eo + t
    p = 1 / (1 + np.exp(-e / K))
    target = (y + 1) / 2
    eps = 1e-9
    loss = -np.mean(target * np.log(p + eps) + (1 - target) * np.log(1 - p + eps))
    d = (p - target) / K / len(y)
    gw = fm.T @ d - s * (fo.T @ d)
    gs = -(eo @ d)
    gt = d.sum()
    return loss + l2 * np.sum((w - theta0[:-2]) ** 2) / np.sum(theta0[:-2] ** 2 + 1), \
        np.concatenate([gw + 2 * l2 * (w - theta0[:-2]) / np.sum(theta0[:-2] ** 2 + 1), [gs, gt]])


ap = argparse.ArgumentParser()
ap.add_argument("games", nargs="+")
ap.add_argument("--rule", default="renju")
ap.add_argument("--iters", type=int, default=4000)
ap.add_argument("--lr", type=float, default=0.5)
ap.add_argument("--l2", type=float, default=0.01, help="今の重みから離れすぎない正則化")
a = ap.parse_args()

gid, y, fm, fo = load(a.games, a.rule)
print("positions: %d (games %d)" % (len(y), len(set(gid))))
w0, s0, t0 = current_params()
theta0 = np.concatenate([w0, [s0, t0]])
# 対局単位で 8:2 に分ける
rng = np.random.default_rng(1)
games = np.array(sorted(set(gid)))
val_games = set(rng.choice(games, size=max(1, len(games) // 5), replace=False))
vm = np.array([g in val_games for g in gid])

# K: 今の重みで損失が最小になる尺度
best = None
for K in [50, 100, 150, 200, 300, 400, 600, 800, 1200]:
    l, _ = loss_grad(theta0, fm[~vm], fo[~vm], y[~vm], K, 0)
    if best is None or l < best[0]:
        best = (l, K)
K = best[1]
print("K = %d, loss(current) train %.5f val %.5f" % (K, best[0], loss_grad(theta0, fm[vm], fo[vm], y[vm], K, 0)[0]))

# Adam
theta = theta0.copy()
scale = np.maximum(np.abs(theta0), 5.0)   # 各重みの大きさに応じた刻み
scale[-2] = 0.1
m = np.zeros_like(theta); v = np.zeros_like(theta)
for it in range(1, a.iters + 1):
    l, g = loss_grad(theta, fm[~vm], fo[~vm], y[~vm], K, a.l2)
    g = g * scale
    m = 0.9 * m + 0.1 * g; v = 0.999 * v + 0.001 * g * g
    step = a.lr * 0.01 * (m / (1 - 0.9 ** it)) / (np.sqrt(v / (1 - 0.999 ** it)) + 1e-8)
    theta -= step * scale
    theta[:-2] = np.maximum(theta[:-2], 0)
    theta[-2] = min(max(theta[-2], 0.3), 1.5)
    if it % 1000 == 0:
        print("iter %d train %.5f val %.5f" % (it, l, loss_grad(theta, fm[vm], fo[vm], y[vm], K, 0)[0]))

w = np.round(theta[:-2]).astype(int)
print("\nline :", " ".join("%s=%d->%d" % (n, o, x) for n, o, x in zip(LINE_NAMES, w0[:NP], w[:NP])))
print("combo:", " ".join("%s=%d->%d" % (n, o, x) for n, o, x in zip(P4_NAMES, w0[NP:], w[NP:])))
print("opp_scale %d -> %d, tempo %d -> %d" % (s0 * 64, round(theta[-2] * 64), t0, round(theta[-1])))
info = ["p_line%d=%d" % (i, x) for i, x in enumerate(w[:NP])] + ["p_combo%d=%d" % (i, x) for i, x in enumerate(w[NP:])]
info += ["p_opp_scale=%d" % round(theta[-2] * 64), "p_tempo=%d" % round(theta[-1])]
print("\nmatch.py spec suffix:\n+" + "+".join(info))
print("\neval.c:\n    .line = {%s},\n    .combo = {%s},\n    .opp_scale = %d,\n    .tempo = %d," % (
    ", ".join(map(str, w[:NP])), ", ".join(map(str, w[NP:])), round(theta[-2] * 64), round(theta[-1])))
