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
THREAT_NAMES = ["B4F3", "F3x2", "F3", "B4+", "B4x2", "B3+", "F2x2", "F3many"]
NTK = len(THREAT_NAMES)
NT = 2 * 2 * NTK          # threat[手番の色][手番側/相手側][種類]
NW = NP + NP4             # 空点の和に掛かる重み


def current_params():
    src = open(os.path.join(ENGINE, "src", "eval.c")).read()
    def arr(name):
        i = src.index("." + name + " = {")
        return [int(v) for v in src[src.index("{", i) + 1:src.index("}", i)].split(",")]
    def val(name):
        i = src.index("." + name + " = ")
        return int(src[i + len(name) + 4:src.index(",", i)])
    thr = [0] * NT
    if ".threat = {" in src:
        i = src.index(".threat = {")
        body = src[i:src.index("}}},", i) + 3]
        thr = [int(v) for v in body.replace("{", " ").replace("}", " ").replace(".threat =", "").replace(",", " ").split()]
    return np.array(arr("line") + arr("combo") + thr, dtype=float), val("opp_scale") / 64.0, float(val("tempo"))


def load(paths, rule):
    """戻り値: 対局番号, 手数, 勝敗 (手番側から), 特徴量 (手番側), 特徴量 (相手側), 各対局の手順の文字列"""
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
    fm, fo = a[:, 4:4 + NW], a[:, 4 + NW:4 + 2 * NW]
    bits = a[:, 4 + 2 * NW:4 + 2 * NW + 2 * NTK]
    # 脅威の特徴: 手番の色ごとに別の重み (黒番の行は前半 10 個、白番は後半 10 個)
    fx = np.zeros((len(a), NT))
    black = a[:, 2] == 1
    fx[black, :2 * NTK] = bits[black]
    fx[~black, 2 * NTK:] = bits[~black]
    return a[:, 0], a[:, 1].astype(int), a[:, 3], np.hstack([fm, fx]), fo, [l.split()[1:] for l in lines]


def rapfi_targets(gid, ply, movestr, n, nodes, cache):
    """局面を n 個選び Rapfi の評価 (手番側から) を付ける。戻り値: 選んだ行の添字, 勝つ確率の目標値。
    詰みは 0/1、それ以外は sigmoid(評価 / RAPFI_K)。結果は cache に保存して使い回す"""
    import tempfile
    rng = np.random.default_rng(7)
    idx = np.sort(rng.choice(len(gid), size=min(n, len(gid)), replace=False))
    pos = [" ".join(movestr[int(gid[i])][:ply[i]]) for i in idx]
    labels = {}
    if cache and os.path.exists(cache):
        for l in open(cache):
            k, v = l.rstrip("\n").split("\t")
            labels[k] = v
    todo = [p for p in pos if p not in labels]
    if todo:
        # 1 局面ずつキャッシュに追記する (途中で止まっても続きから再開できる)
        with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
            f.write("\n".join(todo) + "\n")
        proc = subprocess.Popen([sys.executable, os.path.join(HERE, "rapfi_label.py"), "--nodes", str(nodes), f.name],
                                stdout=subprocess.PIPE, text=True)
        with open(cache, "a") as out:
            for k, (p, r) in enumerate(zip(todo, proc.stdout)):
                labels[p] = r.split()[0] if r.split() else "None"
                out.write("%s\t%s\n" % (p, labels[p]))
                out.flush()
                if (k + 1) % 500 == 0:
                    print("labeled %d / %d" % (k + 1, len(todo)), flush=True)
        proc.wait()
    keep, target = [], []
    for i, p in zip(idx, pos):
        ev = labels.get(p)
        if ev is None or ev in ("None", "error"):
            continue
        if "M" in ev:
            t = 1.0 if ev.startswith("+") or ev[0] == "M" else 0.0
        else:
            t = 1 / (1 + np.exp(-int(ev) / RAPFI_K))
        keep.append(i)
        target.append(t)
    return np.array(keep), np.array(target)


RAPFI_K = 300.0


def loss_grad(theta, fm, fo, y, K, l2):
    """y は勝敗 (-1/0/1) か、Rapfi ラベルのときは (2p-1) (p = 目標の勝つ確率)"""
    # fm = [手番側の空点の特徴 NW 個, 脅威の特徴 NT 個]、fo = 相手側の空点の特徴 NW 個
    w, s, t = theta[:-2], theta[-2], theta[-1]
    em, eo = fm @ w, fo @ w[:NW]
    e = em - s * eo + t
    p = 1 / (1 + np.exp(-e / K))
    target = (y + 1) / 2
    eps = 1e-9
    loss = -np.mean(target * np.log(p + eps) + (1 - target) * np.log(1 - p + eps))
    d = (p - target) / K / len(y)
    gw = fm.T @ d
    gw[:NW] -= s * (fo.T @ d)
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
ap.add_argument("--rapfi", type=int, default=0, help="勝敗の代わりに Rapfi の評価を正解にする局面の数")
ap.add_argument("--rapfi-nodes", type=int, default=20000)
ap.add_argument("--cache", default=os.path.join(HERE, "..", "build", "rapfi_labels.tsv"))
ap.add_argument("--fixed", default="0,1", help="調整しない線パターンの番号 (既定 DEAD, OL)")
ap.add_argument("--fix-tempo", action="store_true")
a = ap.parse_args()

gid, ply, y, fm, fo, movestr = load(a.games, a.rule)
if a.rapfi:
    keep, t = rapfi_targets(gid, ply, movestr, a.rapfi, a.rapfi_nodes, a.cache)
    gid, y, fm, fo = gid[keep], 2 * t - 1, fm[keep], fo[keep]
    print("rapfi labels: %d (mean p %.3f)" % (len(y), t.mean()))
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
for K in [50, 100, 150, 200, 300, 400, 600, 800, 1200, 1600, 2400, 3200]:
    l, _ = loss_grad(theta0, fm[~vm], fo[~vm], y[~vm], K, 0)
    if best is None or l < best[0]:
        best = (l, K)
K = best[1]
print("K = %d, loss(current) train %.5f val %.5f" % (K, best[0], loss_grad(theta0, fm[vm], fo[vm], y[vm], K, 0)[0]))

# Adam
fixed = [int(v) for v in a.fixed.split(",") if v != ""]
fixed += [NP + P4_NAMES.index(n) for n in ("F4", "FIVE")]   # 静かな局面には現れない
fixed += [9, 10, 11]                                          # 線の F4/D4/F5 も同じ
if a.fix_tempo:
    fixed.append(len(theta0) - 1)
theta = theta0.copy()
scale = np.maximum(np.abs(theta0), 5.0)   # 各重みの大きさに応じた刻み
scale[NW:NW + NT] = np.maximum(scale[NW:NW + NT], 50.0)
scale[-2] = 0.1
m = np.zeros_like(theta); v = np.zeros_like(theta)
for it in range(1, a.iters + 1):
    l, g = loss_grad(theta, fm[~vm], fo[~vm], y[~vm], K, a.l2)
    g = g * scale
    g[fixed] = 0
    m = 0.9 * m + 0.1 * g; v = 0.999 * v + 0.001 * g * g
    step = a.lr * 0.01 * (m / (1 - 0.9 ** it)) / (np.sqrt(v / (1 - 0.999 ** it)) + 1e-8)
    theta -= step * scale
    theta[:NW] = np.maximum(theta[:NW], 0)   # 空点の重みは負にしない (脅威の加点は負も可)
    theta[-2] = min(max(theta[-2], 0.3), 1.5)
    if it % 1000 == 0:
        print("iter %d train %.5f val %.5f" % (it, l, loss_grad(theta, fm[vm], fo[vm], y[vm], K, 0)[0]))

w = np.round(theta[:-2]).astype(int)
print("\nline :", " ".join("%s=%d->%d" % (n, o, x) for n, o, x in zip(LINE_NAMES, w0[:NP], w[:NP])))
print("combo:", " ".join("%s=%d->%d" % (n, o, x) for n, o, x in zip(P4_NAMES, w0[NP:NW], w[NP:NW])))
for c in range(2):
    for side in range(2):
        j = NW + (c * 2 + side) * NTK
        print("threat %s %s:" % ("黒番" if c == 0 else "白番", "手番側" if side == 0 else "相手側"),
              " ".join("%s=%d->%d" % (n, o, x) for n, o, x in zip(THREAT_NAMES, theta0[j:j + NTK], w[j:j + NTK])))
print("opp_scale %d -> %d, tempo %d -> %d" % (s0 * 64, round(theta[-2] * 64), t0, round(theta[-1])))
info = ["p_line%d=%d" % (i, x) for i, x in enumerate(w[:NP])] + ["p_combo%d=%d" % (i, x) for i, x in enumerate(w[NP:NW])]
info += ["p_threat%d%d%d=%d" % (c, side, k, w[NW + (c * 2 + side) * NTK + k])
         for c in range(2) for side in range(2) for k in range(NTK)]
info += ["p_opp_scale=%d" % round(theta[-2] * 64), "p_tempo=%d" % round(theta[-1])]
print("\nmatch.py spec suffix:\n+" + "+".join(info))
thr = w[NW:NW + NT].reshape(2, 2, NTK)
print("\neval.c:\n    .line = {%s},\n    .combo = {%s},\n    .opp_scale = %d,\n    .tempo = %d,\n    .threat = {%s}," % (
    ", ".join(map(str, w[:NP])), ", ".join(map(str, w[NP:NW])), round(theta[-2] * 64), round(theta[-1]),
    ", ".join("{%s}" % ", ".join("{%s}" % ", ".join(map(str, r)) for r in thr[c]) for c in range(2))))
