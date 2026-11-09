#!/usr/bin/env python3
"""形勢の問題集: drops.py が出した「Rapfi の評価を大きく下げた G2 の手」の局面を G2 に解かせ、選んだ手の質を Rapfi で測る。

  python3 engine/tests/position_test.py drops.tsv [--engine path] [--tc 1000] [--info p_x=1 ...] [-j 4] [-v]

各局面で G2 の手を打った後の局面を Rapfi で評価し (drops.py と同じ --cache を使い回す)、
「打つ前の評価 − 打った後の評価 (G2 から見た値)」(損失) を出す。詰みは ±20000。
出力: 損失の平均 (上限 1000 で切る)、損失 150 未満の割合、Rapfi と同じ手の数、元の悪い手と同じ手の数。
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
ap.add_argument("puzzles")
ap.add_argument("--engine", default=os.path.join(HERE, "..", "build", "pbrain-g2"))
ap.add_argument("--tc", type=int, default=1000)
ap.add_argument("--info", nargs="*", default=[])
ap.add_argument("--nodes", type=int, default=300000)
ap.add_argument("--cache", default=os.path.join(HERE, "..", "build", "drops_labels.tsv"))
ap.add_argument("-j", type=int, default=4)
ap.add_argument("-v", action="store_true")
a = ap.parse_args()


def val(ev):
    if ev in ("None", "error", None):
        return None
    if "M" in ev:
        return -20000 if ev.startswith("-") else 20000
    return int(ev)


def think(line):
    moves = [tuple(map(int, m.split(","))) for m in line.split()]
    eng = Engine([os.path.abspath(a.engine)], name="g2")
    try:
        eng.start(15, a.tc, RULE_GOMOCUP_CODE["renju"], dict(kv.split("=", 1) for kv in a.info))
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
    todo = sorted(set(k for k in [p[0] for p in puzzles] + [p[0] + " " + m for p, m in zip(puzzles, answers)
                                                            if not m.startswith("error")] if k not in cache))
    if todo:
        rapfi_label.ARGS = Namespace(rule="renju", nodes=a.nodes)
        with Pool(a.j) as pool, open(a.cache, "a") as f:
            for k, lab in zip(todo, pool.imap(rapfi_label.label, todo)):
                cache[k] = lab
                f.write("%s\t%s\n" % (k, lab))
    losses, good, same_r, same_bad = [], 0, 0, 0
    for p, mv in zip(puzzles, answers):
        vb = val(cache.get(p[0], "None").split()[0])
        va = val(cache.get(p[0] + " " + mv, "None").split()[0]) if not mv.startswith("error") else None
        loss = 1000 if vb is None or va is None else max(0, min(1000, vb + va))
        losses.append(loss)
        good += loss < 150
        same_r += mv == p[2]
        same_bad += mv == p[1]
        if a.v:
            print("%5d %-6s bad %-6s rapfi %-6s  %s" % (loss, mv, p[1], p[2], p[6] if len(p) > 6 else ""))
    n = len(puzzles)
    print("position: avg loss %.0f, ok(<150) %d / %d, =rapfi %d, =old-bad %d (tc %d, info %s)" %
          (sum(losses) / n, good, n, same_r, same_bad, a.tc, " ".join(a.info) or "-"))


if __name__ == "__main__":
    main()
