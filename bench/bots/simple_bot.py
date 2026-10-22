#!/usr/bin/env python3
"""弱い参照ボット (Gomocup プロトコル)。強さの下限の目安と、ハーネスの動作確認用。

  --mode random : 既存の石の近く (距離2以内) にランダムに打つ
  --mode greedy : 1手読み。自分の勝ち > 相手の勝ち阻止 > 自他の連の伸びの合計点 で選ぶ
  --renju       : 黒番のとき連珠の禁じ手を避ける
"""

import argparse
import os
import random
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from rules import BLACK, DIRS, EMPTY, N, WHITE, inside, renju_forbidden_reason  # noqa: E402

LINE_SCORE = {5: 100000, 4: 5000, 3: 500, 2: 50, 1: 5}


def line_value(board, x, y, dx, dy, color):
    """(x,y) に color を置いたときの、その方向の連と両端の開きから点数化。"""
    n = 1
    open_ends = 0
    for s in (1, -1):
        cx, cy = x + s * dx, y + s * dy
        while inside(cx, cy) and board[cy][cx] == color:
            n += 1
            cx += s * dx
            cy += s * dy
        if inside(cx, cy) and board[cy][cx] == EMPTY:
            open_ends += 1
    if n >= 5:
        return LINE_SCORE[5]
    if open_ends == 0:
        return 0
    return LINE_SCORE[n] * (2 if open_ends == 2 else 1)


def candidates(board):
    stones = [(x, y) for y in range(N) for x in range(N) if board[y][x] != EMPTY]
    if not stones:
        return [(N // 2, N // 2)]
    out = set()
    for x, y in stones:
        for dy in range(-2, 3):
            for dx in range(-2, 3):
                cx, cy = x + dx, y + dy
                if inside(cx, cy) and board[cy][cx] == EMPTY:
                    out.add((cx, cy))
    return sorted(out)


def choose(board, me, mode, renju, rng):
    cands = candidates(board)
    if renju and me == BLACK:
        legal = [c for c in cands if renju_forbidden_reason(board, c[0], c[1]) is None]
        cands = legal or cands
    if mode == "random":
        return rng.choice(cands)
    opp = 3 - me
    best, best_v = None, -1
    for x, y in cands:
        mine = sum(line_value(board, x, y, dx, dy, me) for dx, dy in DIRS)
        theirs = sum(line_value(board, x, y, dx, dy, opp) for dx, dy in DIRS)
        v = mine * 1.1 + theirs + rng.random()
        if v > best_v:
            best, best_v = (x, y), v
    return best


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mode", default="greedy", choices=["random", "greedy"])
    ap.add_argument("--renju", action="store_true")
    ap.add_argument("--seed", type=int)
    args = ap.parse_args()
    rng = random.Random(args.seed)
    board = [[EMPTY] * N for _ in range(N)]

    def count():
        return sum(1 for row in board for v in row if v != EMPTY)

    def reply():
        me = BLACK if count() % 2 == 0 else WHITE
        x, y = choose(board, me, args.mode, args.renju, rng)
        board[y][x] = me
        print("%d,%d" % (x, y), flush=True)

    for line in sys.stdin:
        cmd = line.strip()
        u = cmd.upper()
        if u.startswith("START"):
            board = [[EMPTY] * N for _ in range(N)]
            print("OK" if int(cmd.split()[1]) == N else "ERROR size", flush=True)
        elif u.startswith("RESTART"):
            board = [[EMPTY] * N for _ in range(N)]
            print("OK", flush=True)
        elif u.startswith("INFO"):
            continue
        elif u.startswith("BEGIN"):
            reply()
        elif u.startswith("TURN"):
            x, y = (int(v) for v in cmd.split()[1].split(","))
            board[y][x] = BLACK if count() % 2 == 0 else WHITE
            reply()
        elif u.startswith("BOARD"):
            stones = []
            for l2 in sys.stdin:
                l2 = l2.strip()
                if l2.upper().startswith("DONE"):
                    break
                x, y, c = (int(v) for v in l2.split(","))
                stones.append((x, y, c))
            board = [[EMPTY] * N for _ in range(N)]
            me = BLACK if len(stones) % 2 == 0 else WHITE
            for x, y, c in stones:
                board[y][x] = me if c == 1 else 3 - me
            reply()
        elif u.startswith("ABOUT"):
            print('name="simple_bot-%s"' % args.mode, flush=True)
        elif u.startswith("END"):
            break


if __name__ == "__main__":
    main()
