#!/usr/bin/env python3
"""vcf_defenses の取りこぼしを調べる (連珠)。

各局面で「相手 (手番でない側) に VCF がある」とき、手番側の合法手をすべて打ってみて
相手の VCF が消える手を総当たりで求め、vcf_defenses がそれを全部返しているかを確かめる。

  python3 engine/tests/check_defenses.py positions.txt
"""

import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "..", "bench"))
from rules import renju_forbidden_reason  # noqa: E402
from tss_verify import parse_moves  # noqa: E402

TSS = os.path.join(HERE, "..", "build", "g2-tss")


def main():
    pos = [l.split("#")[0].strip() for l in open(sys.argv[1]) if l.split("#")[0].strip()]
    d = subprocess.run([TSS, "defend", "renju", "200000"], input="\n".join(pos) + "\n",
                       capture_output=True, text=True).stdout.splitlines()
    lines, meta = [], []
    for k, (p, r) in enumerate(zip(pos, d)):
        if r.startswith("none"):
            continue
        mv = parse_moves(p)
        occ = set(mv)
        board = [[0] * 15 for _ in range(15)]
        for i, (x, y) in enumerate(mv):
            board[y][x] = 1 if i % 2 == 0 else 2
        black = len(mv) % 2 == 0
        for y in range(15):
            for x in range(15):
                if (x, y) in occ or (black and renju_forbidden_reason(board, x, y)):
                    continue
                lines.append(p + " %d,%d" % (x, y))
                meta.append((k, (x, y)))
    out = subprocess.run([TSS, "solve", "renju", "vcf", "30", "200000"], input="\n".join(lines) + "\n",
                         capture_output=True, text=True).stdout.splitlines()
    good = {}
    for (k, m), r in zip(meta, out):
        if not r.startswith("win"):
            good.setdefault(k, set()).add(m)
    miss = checked = 0
    for k, (p, r) in enumerate(zip(pos, d)):
        if r.startswith("none"):
            continue
        checked += 1
        got = {tuple(map(int, t.split(","))) for t in r.split("|")[1].split()}
        lost = good.get(k, set()) - got
        if lost:
            miss += 1
            print("MISS line %d: %s | %s" % (k + 1, sorted(lost), p))
    print("checked %d positions, %d with missed defenses" % (checked, miss))
    return 1 if miss else 0


if __name__ == "__main__":
    sys.exit(main())
