#!/usr/bin/env python3
"""受けの問題集: G2 が「負けの決まる手」を避けられるかを数える。

  python3 engine/tests/defense_test.py [--puzzles engine/tests/defense_renju.txt] [--engine path] [--tc 1000] \
      [--info p_vctdef_pct=15 ...] [-j 4]

問題集の各行は "手順<TAB>負けの手<TAB>受かる手<TAB>メモ" (手番側が受ける側)。
G2 に考えさせた手が「受かる手」なら正解、「負けの手」なら不正解、それ以外は打った後の局面を Rapfi で
評価して、相手に詰み (+M) がなければ正解とする (評価は --cache に保存して使い回す)。
問題集は tests/make_defense_puzzles.py で負けた対局から作る。
"""
import argparse, os, sys, time
from argparse import Namespace
from multiprocessing import Pool

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "bench"))
sys.path.insert(0, HERE)
from engine import Engine  # noqa: E402
from rules import RULE_GOMOCUP_CODE  # noqa: E402
import rapfi_label  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument("--puzzles", default=os.path.join(HERE, "defense_renju.txt"))
ap.add_argument("--engine", default=os.path.join(HERE, "..", "build", "pbrain-g2"))
ap.add_argument("--tc", type=int, default=1000)
ap.add_argument("--rule", default="renju")
ap.add_argument("--info", nargs="*", default=[], help="追加の INFO (key=value)")
ap.add_argument("--nodes", type=int, default=1000000, help="Rapfi で確かめるノード数")
ap.add_argument("--cache", default=os.path.join(HERE, "..", "build", "defense_labels.tsv"))
ap.add_argument("-j", type=int, default=4)
ap.add_argument("-v", action="store_true")
a = ap.parse_args()


def think(line):
    moves = [tuple(map(int, m.split(","))) for m in line.split()]
    eng = Engine([os.path.abspath(a.engine)], name="g2")
    try:
        eng.start(15, a.tc, RULE_GOMOCUP_CODE[a.rule], dict(kv.split("=", 1) for kv in a.info))
        n = len(moves)
        stones = [(x, y, 1 if (i % 2) == (n % 2) else 2) for i, (x, y) in enumerate(moves)]
        x, y, _ = eng.board(stones, time.monotonic() + a.tc / 1000 * 3 + 5)
        return "%d,%d" % (x, y)
    except Exception as e:  # noqa: BLE001
        return "error:%s" % e
    finally:
        eng.close()


def main():
    puzzles = [l.rstrip("\n").split("\t") for l in open(a.puzzles) if l.strip() and not l.startswith("#")]
    with Pool(a.j) as pool:
        answers = pool.map(think, [p[0] for p in puzzles])
    cache = {}
    if os.path.exists(a.cache):
        for l in open(a.cache):
            k, v = l.rstrip("\n").split("\t")
            cache[k] = v
    todo = []
    for p, mv in zip(puzzles, answers):
        if mv not in (p[1], p[2]) and not mv.startswith("error"):
            key = p[0] + " " + mv
            if key not in cache:
                todo.append(key)
    if todo:
        rapfi_label.ARGS = Namespace(rule=a.rule, nodes=a.nodes)
        with Pool(a.j) as pool:
            labs = pool.map(rapfi_label.label, todo)
        with open(a.cache, "a") as f:
            for k, v in zip(todo, labs):
                cache[k] = v.split()[0] if v.split() else "None"
                f.write("%s\t%s\n" % (k, cache[k]))
    ok = bad = 0
    for p, mv in zip(puzzles, answers):
        if mv == p[2]:
            res = "ok(=good)"
        elif mv == p[1]:
            res = "NG(=bad)"
        elif mv.startswith("error"):
            res = "NG(error)"
        else:
            lab = cache.get(p[0] + " " + mv, "None")
            res = "NG(%s)" % lab if lab.startswith("+M") or lab.startswith("M") else "ok(%s)" % lab
        ok += res.startswith("ok")
        bad += res.startswith("NG")
        if a.v:
            print("%-14s %s  bad %s good %s  %s" % (res, mv, p[1], p[2], p[3] if len(p) > 3 else ""))
    print("defense: %d / %d ok (tc %d, info %s)" % (ok, ok + bad, a.tc, " ".join(a.info) or "-"))


if __name__ == "__main__":
    main()
