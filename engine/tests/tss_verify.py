#!/usr/bin/env python3
"""VCF の勝ち手順を bench/rules.py (C とは別実装のレフェリー) で検証する。

  g2-tss solve <rule> vcf の出力と入力局面を突き合わせ、各手順が
  「攻めは合法な四 (または五)・受けは唯一の止め点・最後は攻めの勝ち」になっているかを確かめる。

使い方:
  python3 engine/tests/tss_verify.py --rule renju positions.txt solver_output.txt [--opp]
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "bench"))
from rules import BLACK, DIRS, EMPTY, N, WHITE, inside, renju_forbidden_reason, run_length  # noqa: E402


def wins(board, x, y, color, rule):
    """color が (x,y) (空点) に打つと勝ち (五) か。"""
    board[y][x] = color
    try:
        runs = [run_length(board, x, y, dx, dy, color) for dx, dy in DIRS]
    finally:
        board[y][x] = EMPTY
    if rule == "freestyle":
        return max(runs) >= 5
    if rule == "standard":
        return 5 in runs
    # renju: 黒はちょうど五、白は五以上
    return 5 in runs if color == BLACK else max(runs) >= 5


def five_points(board, color, rule):
    return [(x, y) for y in range(N) for x in range(N) if board[y][x] == EMPTY and wins(board, x, y, color, rule)]


def forbidden(board, x, y, color, rule):
    return rule == "renju" and color == BLACK and renju_forbidden_reason(board, x, y) is not None


def verify(moves, line, rule, opp):
    board = [[EMPTY] * N for _ in range(N)]
    for i, (x, y) in enumerate(moves):
        board[y][x] = BLACK if i % 2 == 0 else WHITE
    side = BLACK if len(moves) % 2 == 0 else WHITE
    att = (3 - side) if opp else side
    dfn = 3 - att
    i = 0
    while i < len(line):
        ax, ay = line[i]
        if board[ay][ax] != EMPTY:
            return "attacker move on occupied point %d,%d" % (ax, ay)
        dfives = five_points(board, dfn, rule)
        if wins(board, ax, ay, att, rule):
            return None
        if forbidden(board, ax, ay, att, rule):
            return "attacker move %d,%d is forbidden" % (ax, ay)
        if dfives and (ax, ay) not in dfives:
            return "defender has five at %s but attacker played %d,%d" % (dfives[0], ax, ay)
        if len(dfives) >= 2:
            return "defender has two fives"
        board[ay][ax] = att
        afives = five_points(board, att, rule)
        if not afives:
            return "attacker move %d,%d is not a four" % (ax, ay)
        if five_points(board, dfn, rule):
            return "defender can make five after %d,%d" % (ax, ay)
        if len(afives) >= 2:
            return None
        fx, fy = afives[0]
        if forbidden(board, fx, fy, dfn, rule):
            return None  # 止め点が黒の禁じ手
        if i + 1 >= len(line):
            return "line ends before win"
        if tuple(line[i + 1]) != (fx, fy):
            return "defender reply %s is not the only block %d,%d" % (line[i + 1], fx, fy)
        board[fy][fx] = dfn
        i += 2
    return "line ends before win"


def parse_moves(s):
    s = s.split("#")[0]
    out = []
    for tok in s.split():
        if "," in tok:
            x, y = tok.split(",")
            out.append((int(x), int(y)))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rule", default="renju", choices=["renju", "freestyle", "standard"])
    ap.add_argument("--opp", action="store_true", help="手番でない側の詰み (g2-tss opp の出力)")
    ap.add_argument("positions")
    ap.add_argument("results")
    args = ap.parse_args()
    pos = [l for l in open(args.positions) if l.strip()]
    res = [l for l in open(args.results) if l.strip()]
    assert len(pos) == len(res), (len(pos), len(res))
    nwin = bad = 0
    for k, (p, r) in enumerate(zip(pos, res)):
        if not r.startswith("win"):
            continue
        nwin += 1
        line = parse_moves(r.split("|")[1])
        err = verify(parse_moves(p), line, args.rule, args.opp)
        if err:
            bad += 1
            print("position %d: %s\n  %s  %s" % (k, err, p.strip(), r.strip()))
    print("verified %d wins, %d invalid" % (nwin, bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
