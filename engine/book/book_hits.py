#!/usr/bin/env python3
"""対局の棋譜 (games.jsonl) で、指定エンジンの手番の局面のうち何手がブックに当たったか・ブックの手と同じ手を指したかを数える。

  python3 engine/book/book_hits.py bench/results/<dir>/games.jsonl --engine g2-book
  python3 engine/book/book_hits.py docs/benchmarks/web/games/gomocalc_5000_renju.jsonl --engine g2
"""

import argparse
import collections
import json
import os
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
EXE = os.path.join(os.path.dirname(HERE), "build", "g2-book")
CODE = {"renju": 4, "freestyle": 0, "standard": 1}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("games", nargs="+")
    ap.add_argument("--engine", default="g2")
    args = ap.parse_args()
    by_ply = collections.Counter()
    hits = collections.Counter()
    same = collections.Counter()
    last_hit = []
    for path in args.games:
        for line in open(path):
            g = json.loads(line)
            if g.get("rule") not in CODE:
                continue
            mv = [tuple(m) for m in g.get("opening", [])] + [tuple(m) for m in g["moves"]]
            mine = 0 if g["black"] == args.engine else 1 if g["white"] == args.engine else None
            if mine is None and g.get("engine") == args.engine:
                mine = 0 if g.get("engine_color") == "black" else 1
            if mine is None:
                continue
            start = len(g.get("opening", []))
            qs = [(i, mv[:i]) for i in range(start, min(len(mv), 16)) if i % 2 == mine]
            inp = "".join("%d %s\n" % (CODE[g["rule"]], " ".join("%d,%d" % m for m in pre)) for _, pre in qs)
            out = subprocess.run([EXE], input=inp, capture_output=True, text=True, check=True).stdout.split()
            lh = -1
            for (i, _), r in zip(qs, out):
                by_ply[i] += 1
                if r != "-":
                    hits[i] += 1
                    lh = i
                    if tuple(int(v) for v in r.split(",")) == mv[i]:
                        same[i] += 1
            last_hit.append(lh)
    print("ply  positions  book-hit  played-book-move")
    for i in sorted(by_ply):
        print("%3d  %9d  %8d  %16d" % (i, by_ply[i], hits[i], same[i]))
    print("games %d, last book move ply: %s" % (len(last_hit), dict(sorted(collections.Counter(last_hit).items()))))


if __name__ == "__main__":
    main()
