#!/usr/bin/env python3
"""負けた対局で、G2 の手が Rapfi の評価をどれだけ下げたか (詰みになる前の「形勢の悪手」) を洗い出す。

  python3 engine/tests/drops.py games.jsonl [--name g2] [--nodes 300000] [--min 150] [-j 4] > drops.tsv

G2 の各手について、打つ前 (G2 の手番) と打った後 (相手の手番) の局面を Rapfi で評価し (--cache に保存して使い回す)、
「打つ前の評価 − 打った後の評価 (G2 から見た値)」が --min 以上の手を出す。詰みの値は ±20000 として扱う。
出力: 手順<TAB>G2 の手<TAB>Rapfi の手<TAB>下がった量<TAB>打つ前<TAB>打った後 (G2 から)<TAB>対局#手数
1 列目と 3 列目は defense_test.py の問題集と同じ形 (手順、悪い手、良い手) なので、そのまま G2 に解かせられる。
"""
import argparse, json, os, sys
from argparse import Namespace
from multiprocessing import Pool

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "bench"))
sys.path.insert(0, HERE)
import rapfi_label  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument("games", nargs="+")
ap.add_argument("--name", default="g2")
ap.add_argument("--nodes", type=int, default=300000)
ap.add_argument("--min", type=int, default=150)
ap.add_argument("--all", action="store_true", help="勝った対局も含める")
ap.add_argument("--rule", default="renju", choices=["renju", "freestyle", "standard"], help="Rapfi に評価させるルール")
ap.add_argument("--cache", default=None, help="既定は engine/cache/drops_labels.tsv (renju)、drops_labels_<rule>.tsv (それ以外)")
ap.add_argument("-j", type=int, default=4)
a = ap.parse_args()
if a.cache is None:
    a.cache = os.path.join(HERE, "..", "cache", "drops_labels.tsv" if a.rule == "renju" else "drops_labels_%s.tsv" % a.rule)


def val(ev):
    """Rapfi の評価 (手番側から) を数に"""
    if ev in ("None", "error", None):
        return None
    if "M" in ev:
        return -20000 if ev.startswith("-") else 20000
    return int(ev)


items = []
for path in a.games:
    for l in open(path):
        g = json.loads(l)
        me = "black" if g["black"].startswith(a.name) else "white" if g["white"].startswith(a.name) else None
        if me is None or (not a.all and g.get("winner") in (None, me, "draw")):
            continue
        mv = ["%d,%d" % tuple(m) for m in g["opening"] + g["moves"]]
        for i, m in enumerate(mv):
            if (i % 2 == 0) != (me == "black") or i < max(1, len(g["opening"])):
                continue
            items.append((" ".join(mv[:i]), m, "%s#%d" % (os.path.basename(os.path.dirname(path)), g["id"]), i + 1))

cache = {}
if os.path.exists(a.cache):
    for l in open(a.cache):
        k, v = l.rstrip("\n").split("\t")
        cache[k] = v
keys = []
for pre, m, _, _ in items:
    for k in (pre, pre + " " + m):
        if k not in cache and k not in keys:
            keys.append(k)
print("positions to label: %d (cached %d)" % (len(keys), len(cache)), file=sys.stderr)
if keys:
    rapfi_label.ARGS = Namespace(rule=a.rule, nodes=a.nodes)
    with Pool(a.j) as pool, open(a.cache, "a") as f:
        for k, lab in zip(keys, pool.imap(rapfi_label.label, keys)):
            cache[k] = lab
            f.write("%s\t%s\n" % (k, lab))
            f.flush()

n = 0
for pre, m, gid, ply in items:
    b, af = cache[pre].split(), cache[pre + " " + m].split()
    if not b or not af:
        continue
    vb, va = val(b[0]), val(af[0])
    if vb is None or va is None:
        continue
    va = -va
    if vb - va >= a.min and vb > -20000:
        rm = b[1] if len(b) > 1 else "?"
        print("%s\t%s\t%s\t%d\t%s\t%d\t%s#%d" % (pre, m, rm, vb - va, b[0], va, gid, ply))
        n += 1
print("drops >= %d: %d / %d moves" % (a.min, n, len(items)), file=sys.stderr)
