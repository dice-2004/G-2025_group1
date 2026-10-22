#!/usr/bin/env python3
"""局面ごとに Rapfi の評価 (手番側から見た値。詰みなら +M<n> / -M<n>) を付ける。VCT の正解ラベル用。

  python3 engine/tests/rapfi_label.py --rule renju --nodes 300000 positions.txt > labels.txt
出力は 1 行 1 局面で "<Eval> <Rapfi の着手>"。Rapfi は bench/setup_rapfi.sh でビルドしておく。
"""

import argparse
import os
import re
import sys
import time
from multiprocessing import Pool

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "bench"))
import match  # noqa: E402
from engine import Engine  # noqa: E402
from rules import RULE_GOMOCUP_CODE  # noqa: E402
from tss_verify import parse_moves  # noqa: E402

EVAL_RE = re.compile(r"Eval ([+-]?M?\d+)")
ARGS = None


def label(line):
    moves = parse_moves(line)
    eng_def = match.resolve_engine("rapfi", match.load_registry())
    log = []
    eng = Engine(eng_def["cmd"], cwd=eng_def.get("cwd"), name="rapfi", log=log, pre_delay=0.05)
    try:
        eng.start(15, 100000, RULE_GOMOCUP_CODE[ARGS.rule], {"max_node": ARGS.nodes})
        n = len(moves)
        stones = [(x, y, 1 if (i % 2) == (n % 2) else 2) for i, (x, y) in enumerate(moves)]
        x, y, _ = eng.board(stones, time.monotonic() + 120)
    except Exception as e:  # noqa: BLE001
        return "error %s" % e
    finally:
        eng.close()
    ev = None
    for l in log:
        m = EVAL_RE.search(l)
        if m:
            ev = m.group(1)
    return "%s %d,%d" % (ev, x, y)


def main():
    global ARGS
    ap = argparse.ArgumentParser()
    ap.add_argument("--rule", default="renju", choices=["renju", "freestyle", "standard"])
    ap.add_argument("--nodes", type=int, default=300000)
    ap.add_argument("-j", type=int, default=4)
    ap.add_argument("positions")
    ARGS = ap.parse_args()
    lines = [l for l in open(ARGS.positions) if l.strip()]
    with Pool(ARGS.j) as pool:
        for r in pool.imap(label, lines):
            print(r, flush=True)


if __name__ == "__main__":
    main()
