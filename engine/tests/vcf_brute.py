#!/usr/bin/env python3
"""遅いが単純な VCF 全探索 (bench/rules.py のルール判定を使う)。C 版の見落とし・誤りの照合用。

  python3 engine/tests/vcf_brute.py --rule renju positions.txt [--opp] [--depth 12] > brute.out
出力は 1 行 1 局面で "win x,y" / "nowin" / "unknown" (ノード上限)。
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tss_verify import forbidden, parse_moves, wins  # noqa: E402
from rules import BLACK, DIRS, EMPTY, N, WHITE, inside  # noqa: E402


def line_five_points(board, x, y, color, rule):
    """(x,y) に color の石がある前提で、その 4 方向 ±4 にある color の五の点。"""
    pts = set()
    for dx, dy in DIRS:
        for k in range(-4, 5):
            qx, qy = x + k * dx, y + k * dy
            if k and inside(qx, qy) and board[qy][qx] == EMPTY and wins(board, qx, qy, color, rule):
                pts.add((qx, qy))
    return pts


def all_five_points(board, color, rule):
    return {(x, y) for y in range(N) for x in range(N) if board[y][x] == EMPTY and wins(board, x, y, color, rule)}


class Solver:
    def __init__(self, rule, max_nodes):
        self.rule = rule
        self.max_nodes = max_nodes
        self.nodes = 0
        self.memo = {}

    def candidates(self, board, att):
        """攻め側が四を作れる点 (石の周り 4 マスの直線上)。"""
        seen = set()
        for y in range(N):
            for x in range(N):
                if board[y][x] != att:
                    continue
                for dx, dy in DIRS:
                    for k in range(-4, 5):
                        qx, qy = x + k * dx, y + k * dy
                        if k and inside(qx, qy) and board[qy][qx] == EMPTY:
                            seen.add((qx, qy))
        return seen

    def attack(self, board, att, depth):
        """攻め側手番。勝てれば初手、勝てなければ None。ノード上限で例外。"""
        self.nodes += 1
        if self.nodes > self.max_nodes:
            raise TimeoutError
        rule = self.rule
        dfn = 3 - att
        afive = all_five_points(board, att, rule)
        if afive:
            return min(afive)
        dfive = all_five_points(board, dfn, rule)
        if len(dfive) >= 2 or depth <= 0:
            return None
        key = (tuple(map(tuple, board)), att)
        if key in self.memo and self.memo[key][0] >= depth:
            return self.memo[key][1]
        cands = dfive if dfive else self.candidates(board, att)
        result = None
        for (x, y) in sorted(cands):
            if forbidden(board, x, y, att, rule):
                continue
            board[y][x] = att
            try:
                fives = line_five_points(board, x, y, att, rule)
                if not fives:
                    continue
                if len(fives) >= 2:
                    result = (x, y)
                    break
                (fx, fy), = fives
                if forbidden(board, fx, fy, dfn, rule):
                    result = (x, y)
                    break
                if all_five_points(board, dfn, rule):
                    continue
                board[fy][fx] = dfn
                try:
                    if self.attack(board, att, depth - 1) is not None:
                        result = (x, y)
                        break
                finally:
                    board[fy][fx] = EMPTY
            finally:
                board[y][x] = EMPTY
        self.memo[key] = (depth if result is None else 999, result)
        return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rule", default="renju", choices=["renju", "freestyle", "standard"])
    ap.add_argument("--opp", action="store_true")
    ap.add_argument("--depth", type=int, default=30)
    ap.add_argument("--nodes", type=int, default=20000)
    ap.add_argument("positions")
    args = ap.parse_args()
    for line in open(args.positions):
        if not line.strip():
            continue
        moves = parse_moves(line)
        board = [[EMPTY] * N for _ in range(N)]
        for i, (x, y) in enumerate(moves):
            board[y][x] = BLACK if i % 2 == 0 else WHITE
        side = BLACK if len(moves) % 2 == 0 else WHITE
        att = 3 - side if args.opp else side
        s = Solver(args.rule, args.nodes)
        try:
            r = s.attack(board, att, args.depth)
            print("win %d,%d" % r if r else "nowin", flush=True)
        except TimeoutError:
            print("unknown", flush=True)


if __name__ == "__main__":
    main()
