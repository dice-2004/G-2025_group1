#!/usr/bin/env python3
"""開局ブックの C 側の表引き (g2-book) を Python 側の正規化 (bookkey.py) と照合する。

book_<rule>.txt の各局面に 8 対称のどれかと手順の入れ替えをかけて引き、返った手が
「元の局面 + ブックの手」と同じ局面 (対称を除いて) になるかを確かめる。

  python3 engine/book/check_book.py [--rounds 3]
"""

import argparse
import os
import random
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from bookkey import canonical, transform  # noqa: E402

EXE = os.path.join(os.path.dirname(HERE), "build", "g2-book")
RULES = {"renju": 4, "freestyle": 0}


def load(rule):
    path = os.path.join(HERE, "book_%s.txt" % rule)
    if not os.path.exists(path):
        return []
    out = []
    for line in open(path):
        if line.startswith("#") or not line.strip():
            continue
        pos, mv, _, _ = line.rstrip("\n").split("\t")
        moves = [] if pos == "-" else [tuple(int(v) for v in t.split(",")) for t in pos.split()]
        out.append((moves, tuple(int(v) for v in mv.split(","))))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rounds", type=int, default=3)
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()
    rng = random.Random(args.seed)
    bad = 0
    for rule, code in RULES.items():
        book = load(rule)
        if not book:
            continue
        cases = []
        for moves, mv in book:
            for _ in range(args.rounds):
                t = rng.randrange(8)
                tm = [transform(t, *m) for m in moves]
                bl, wh = tm[0::2], tm[1::2]
                rng.shuffle(bl)
                rng.shuffle(wh)
                seq = [bl[i // 2] if i % 2 == 0 else wh[i // 2] for i in range(len(tm))]
                cases.append((moves, mv, seq))
        inp = "".join("%d %s\n" % (code, " ".join("%d,%d" % m for m in seq)) for _, _, seq in cases)
        out = subprocess.run([EXE], input=inp, capture_output=True, text=True, check=True).stdout.split()
        hit = miss = 0
        for (moves, mv, seq), got in zip(cases, out):
            if got == "-":
                miss += 1
                continue
            hit += 1
            g = tuple(int(v) for v in got.split(","))
            if canonical(seq + [g])[0] != canonical(moves + [mv])[0]:
                bad += 1
                print("MISMATCH %s: %s -> %s (book %s)" % (rule, seq, g, mv))
        print("book %s: %d entries, %d probes, hit %d, refused %d (threat/forbidden)" %
              (rule, len(book), len(cases), hit, miss))
    if bad:
        sys.exit("book check: %d mismatches" % bad)
    print("book check OK")


if __name__ == "__main__":
    main()
