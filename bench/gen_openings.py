#!/usr/bin/env python3
"""Rapfi の評価値で「互角に近い」開局を選んでファイルに書き出す。

ランダム開局は黒が勝ちすぎて (特に freestyle)、先後入替をしても強さの差が
測りにくい。そこで Rapfi に局面を評価させ、|Eval| が閾値以下のものだけ残す。

例:
  python3 bench/gen_openings.py --rule renju --count 40 --out bench/openings/renju_balanced.txt
"""

import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import match  # noqa: E402
from engine import Engine  # noqa: E402
from rules import BLACK, N, RULE_GOMOCUP_CODE, Referee, renju_forbidden_reason  # noqa: E402

EVAL_RE = re.compile(r"Eval ([+-]?M?\d+)")


def evaluate(engine_def, moves, rule, nodes):
    eng = Engine(engine_def["cmd"], cwd=engine_def.get("cwd"), name="eval", pre_delay=0.05)
    log = []
    eng.log = log
    try:
        eng.start(N, 10000, RULE_GOMOCUP_CODE[rule], {"max_node": nodes})
        stones = []
        to_move = BLACK if len(moves) % 2 == 0 else 3 - BLACK
        for i, (x, y) in enumerate(moves):
            c = BLACK if i % 2 == 0 else 3 - BLACK
            stones.append((x, y, 1 if c == to_move else 2))
        import time
        eng.board(stones, time.monotonic() + 60)
    finally:
        eng.close()
    ev = None
    for line in log:
        m = EVAL_RE.search(line)
        if m:
            ev = m.group(1)
    if ev is None:
        return None
    if "M" in ev:
        return 100000 if not ev.startswith("-") else -100000
    v = int(ev)
    # Rapfi の評価は手番側から見た値。黒視点に直す
    return v if len(moves) % 2 == 0 else -v


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--rule", default="renju", choices=["freestyle", "standard", "renju"])
    ap.add_argument("--count", type=int, default=40)
    ap.add_argument("--moves", type=int, nargs="+", default=[3, 4, 5])
    ap.add_argument("--radius", type=int, default=3)
    ap.add_argument("--threshold", type=int, default=120, help="黒視点 |Eval| の上限")
    ap.add_argument("--nodes", type=int, default=300000)
    ap.add_argument("--engine", default="rapfi")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    eng = match.resolve_engine(args.engine, match.load_registry())
    found = []
    tried = 0
    seed = args.seed
    while len(found) < args.count:
        nm = args.moves[tried % len(args.moves)]
        cand = match.random_openings(1, nm, seed, args.radius)[0]
        seed += 1
        tried += 1
        # 開局途中で終局や禁じ手が無いこと
        ref = Referee(args.rule if args.rule != "renju" else "renju")
        ok = True
        for i, (x, y) in enumerate(cand):
            if args.rule == "renju" and i % 2 == 0 and renju_forbidden_reason(ref.board, x, y):
                ok = False
                break
            done, _, _ = ref.play(x, y)
            if done:
                ok = False
                break
        if not ok:
            continue
        ev = evaluate(eng, cand, args.rule, args.nodes)
        if ev is None:
            continue
        mark = "keep" if abs(ev) <= args.threshold else "skip"
        print("[%d tried, %d kept] %s eval(black)=%s %s" % (
            tried, len(found), " ".join("%d,%d" % m for m in cand), ev, mark), flush=True)
        if mark == "keep":
            found.append((cand, ev))
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w") as f:
        f.write("# rule=%s, Rapfi(%s, %d nodes) で |黒視点Eval|<=%d の開局。1行1開局、黒から順に x,y\n" % (
            args.rule, args.engine, args.nodes, args.threshold))
        for cand, ev in found:
            f.write("%s  # eval %+d\n" % (" ".join("%d,%d" % m for m in cand), ev))
    print("wrote", args.out, "(%d/%d kept)" % (len(found), tried))


if __name__ == "__main__":
    main()
