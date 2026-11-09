#!/usr/bin/env python3
"""blunders.py の出力から受けの問題集 (defense_test.py 用) を作る。

  python3 engine/tests/make_defense_puzzles.py blunders.tsv name=games.jsonl [name=games.jsonl ...] \
      [--nodes 1000000] > engine/tests/defense_renju.txt

blunders.tsv の各行は "<名前>\\t対局\\t手数\\tG2 の手\\tRapfi の手\\t打つ前\\t打った後" (blunders.py の出力の先頭に
エンジン名を付けたもの)。G2 と Rapfi の手が違う局面について、Rapfi の手を打った後の局面を --nodes で評価し直し、
相手に詰みがない (= Rapfi の手で受かる) ものだけを問題にする。
"""
import argparse, json, os, sys
from argparse import Namespace
from multiprocessing import Pool

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "bench"))
sys.path.insert(0, HERE)
import rapfi_label  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument("blunders")
ap.add_argument("games", nargs="+", help="名前=games.jsonl")
ap.add_argument("--nodes", type=int, default=1000000)
ap.add_argument("--rule", default="renju")
ap.add_argument("-j", type=int, default=4)
a = ap.parse_args()

games = {}
for spec in a.games:
    name, path = spec.split("=", 1)
    for l in open(path):
        g = json.loads(l)
        games[(name, g["id"])] = ["%d,%d" % tuple(m) for m in g["opening"] + g["moves"]]

cands = []
for l in open(a.blunders):
    f = l.rstrip("\n").split("\t")
    if len(f) < 7 or f[1] == "game" or f[3] == f[4] or f[4] == "?":
        continue
    name, gid, ply, gm, rm, before, after = f[:7]
    mv = games[(name, int(gid))]
    ply = int(ply)
    if mv[ply - 1] != gm:
        print("skip (mismatch) %s %s %d" % (name, gid, ply), file=sys.stderr)
        continue
    pre = " ".join(mv[:ply - 1])
    cands.append((pre, gm, rm, "%s#%s ply%d before %s after %s" % (name, gid, ply, before, after)))

rapfi_label.ARGS = Namespace(rule=a.rule, nodes=a.nodes)
with Pool(a.j) as pool:
    labs = pool.map(rapfi_label.label, [c[0] + " " + c[2] for c in cands])
n = 0
seen = set()
for (pre, gm, rm, note), lab in zip(cands, labs):
    ev = lab.split()[0] if lab.split() else "None"
    if ev in ("None", "error") or ev.startswith("+M") or ev.startswith("M"):
        continue
    if pre in seen:
        continue
    seen.add(pre)
    print("%s\t%s\t%s\t%s good-after %s" % (pre, gm, rm, note, ev))
    n += 1
print("puzzles: %d / %d candidates" % (n, len(cands)), file=sys.stderr)
