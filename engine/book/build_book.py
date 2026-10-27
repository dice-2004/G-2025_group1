#!/usr/bin/env python3
"""engine/book/book_<rule>.txt (gen_book.py の出力) から engine/src/book_data.c を作る。

  python3 engine/book/build_book.py
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from bookkey import canonical  # noqa: E402

OUT = os.path.join(os.path.dirname(HERE), "src", "book_data.c")


def load(rule):
    path = os.path.join(HERE, "book_%s.txt" % rule)
    entries = {}
    maxst = 0
    if not os.path.exists(path):
        return entries, maxst
    for line in open(path):
        if line.startswith("#") or not line.strip():
            continue
        pos, mv, ev, _ = line.rstrip("\n").split("\t")
        moves = [] if pos == "-" else [tuple(int(v) for v in t.split(",")) for t in pos.split()]
        key, t = canonical(moves)
        assert t == 0, "book_%s.txt の局面が正規形でない: %s" % (rule, pos)
        x, y = (int(v) for v in mv.split(","))
        assert (x, y) not in moves
        entries[key] = (x, y, max(-32000, min(32000, int(ev))))
        maxst = max(maxst, len(moves))
    return entries, maxst


def main():
    out = ["// 自動生成 (engine/book/build_book.py)。手で編集しない", '#include "book_data.h"', ""]
    maxst = 0
    for rule in ("renju", "freestyle"):
        entries, m = load(rule)
        maxst = max(maxst, m)
        out.append("const BookEntry g_book_%s[] = {" % rule)
        for key in sorted(entries):
            x, y, ev = entries[key]
            out.append("    {0x%016xULL, %d, %d, %d}," % (key, x, y, ev))
        if not entries:
            out.append("    {0, 0, 0, 0},")
        out.append("};")
        out.append("const int g_book_%s_n = %d;" % (rule, len(entries)))
        out.append("")
        print("%s: %d entries" % (rule, len(entries)))
    out.append("const int g_book_max_stones = %d;" % maxst)
    with open(OUT, "w") as f:
        f.write("\n".join(out) + "\n")
    print("wrote", OUT)


if __name__ == "__main__":
    main()
