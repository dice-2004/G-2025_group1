#!/usr/bin/env python3
"""ソルバの結果を Rapfi の評価 (rapfi_label.py の出力) と突き合わせる。

  python3 engine/tests/tss_compare.py labels.txt vcf.out vct.out [--positions pos.txt]

- 再現率: Rapfi が「手番側の詰み (+M)」と言う局面のうち、VCF / VCT が勝ちを見つけた割合 (詰み手数別)
- 矛盾: VCF / VCT が勝ちと言うのに Rapfi が「手番側の負け (-M)」と言う局面 (あればバグの疑い)
"""

import argparse
import collections


def parse_label(l):
    ev = l.split()[0] if l.strip() else "None"
    if "M" in ev:
        n = int(ev.replace("+", "").replace("-", "").replace("M", ""))
        return ("win" if not ev.startswith("-") else "loss"), n
    return "eval", None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("labels")
    ap.add_argument("outs", nargs="+", help="g2-tss solve の出力 (名前=ファイル も可)")
    ap.add_argument("--positions")
    args = ap.parse_args()
    labels = [parse_label(l) for l in open(args.labels)]
    pos = [l.strip() for l in open(args.positions)] if args.positions else None
    outs = []
    for o in args.outs:
        name, _, path = o.partition("=") if "=" in o else (o, "", o)
        outs.append((name, [l.startswith("win") for l in open(path)], [("aborted" in l) for l in open(path)]))
    n = min(len(labels), *(len(o[1]) for o in outs))
    buckets = collections.OrderedDict((k, [0] + [0] * len(outs)) for k in ("M1-5", "M7-11", "M13-19", "M21+"))
    contra = []
    other_wins = [0] * len(outs)
    for i in range(n):
        kind, m = labels[i]
        if kind == "win":
            k = "M1-5" if m <= 5 else "M7-11" if m <= 11 else "M13-19" if m <= 19 else "M21+"
            buckets[k][0] += 1
            for j, (_, w, _) in enumerate(outs):
                buckets[k][j + 1] += w[i]
        else:
            for j, (name, w, _) in enumerate(outs):
                if w[i]:
                    if kind == "loss":
                        contra.append((name, i))
                    else:
                        other_wins[j] += 1
    print("| Rapfi の詰み手数 | 局面数 | " + " | ".join(o[0] for o in outs) + " |")
    print("|---|---|" + "---|" * len(outs))
    tot = [0] * (len(outs) + 1)
    for k, v in buckets.items():
        tot = [a + b for a, b in zip(tot, v)]
        print("| %s | %d | " % (k, v[0]) + " | ".join("%d (%.0f%%)" % (x, 100.0 * x / v[0] if v[0] else 0) for x in v[1:]) + " |")
    print("| 計 | %d | " % tot[0] + " | ".join("%d (%.0f%%)" % (x, 100.0 * x / tot[0] if tot[0] else 0) for x in tot[1:]) + " |")
    print()
    print("Rapfi が詰みと言わない局面での勝ち検出: " + ", ".join("%s %d" % (o[0], c) for o, c in zip(outs, other_wins)))
    print("矛盾 (ソルバは勝ち、Rapfi は手番側の負け): %d" % len(contra))
    for name, i in contra[:20]:
        print("  %s line %d %s" % (name, i + 1, pos[i] if pos else ""))


if __name__ == "__main__":
    main()
