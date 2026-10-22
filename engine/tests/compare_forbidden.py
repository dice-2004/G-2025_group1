#!/usr/bin/env python3
"""G2 の連珠禁じ手判定を bench/rules.py (Python 版、素直な定義どおりの実装) と照合する。

ランダムな局面 (黒番) を作り、全空点について「黒が打つと禁じ手か」を両方で判定して比べる。
禁じ手が出やすいよう、黒石を多めに置いた局面も混ぜる。
"""
import argparse
import os
import random
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "bench"))
from rules import BLACK, EMPTY, N, WHITE, max_run, renju_forbidden_reason  # noqa: E402


def random_position(rng):
    """黒番の局面を手順つきで作る (五ができたら作り直す)。"""
    while True:
        board = [[EMPTY] * N for _ in range(N)]
        moves = []
        n = rng.choice([10, 20, 30, 40, 60])
        span = rng.choice([3, 4, 5, 7])
        ok = True
        for i in range(n):
            color = BLACK if i % 2 == 0 else WHITE
            for _ in range(200):
                x = 7 + rng.randint(-span, span)
                y = 7 + rng.randint(-span, span)
                if board[y][x] == EMPTY:
                    break
            else:
                ok = False
                break
            # 白は少し離れた所に打つことがある (黒の形を残すため)
            if color == WHITE and rng.random() < 0.5:
                for _ in range(200):
                    x, y = rng.randrange(N), rng.randrange(N)
                    if board[y][x] == EMPTY:
                        break
            board[y][x] = color
            moves.append((x, y))
            if max_run(board, x, y, color) >= 5:
                ok = False
                break
        if ok and len(moves) % 2 == 0:
            return board, moves


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--positions", type=int, default=300)
    ap.add_argument("--seed", type=int, default=7)
    ap.add_argument("--bin", default=os.path.join(HERE, "..", "build", "g2-test"))
    args = ap.parse_args()
    rng = random.Random(args.seed)
    positions = [random_position(rng) for _ in range(args.positions)]
    inp = "\n".join(" ".join("%d,%d" % m for m in mv) for _, mv in positions) + "\n"
    out = subprocess.run([args.bin, "forbid", "4"], input=inp, capture_output=True, text=True, check=True).stdout
    lines = out.split("\n")
    total = mism = forb = 0
    shown = 0
    for (board, moves), line in zip(positions, lines):
        c_set = set(tuple(int(v) for v in t.split(",")) for t in line.split())
        for y in range(N):
            for x in range(N):
                if board[y][x] != EMPTY:
                    continue
                total += 1
                py = renju_forbidden_reason(board, x, y) is not None
                forb += py
                if py != ((x, y) in c_set):
                    mism += 1
                    if shown < 5:
                        shown += 1
                        print("MISMATCH at %d,%d python=%s c=%s moves=%s" % (
                            x, y, renju_forbidden_reason(board, x, y), (x, y) in c_set,
                            " ".join("%d,%d" % m for m in moves)))
    print("checked %d cells in %d positions: %d forbidden (python), %d mismatches" % (total, len(positions), forb, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
