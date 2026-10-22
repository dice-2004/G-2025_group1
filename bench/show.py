#!/usr/bin/env python3
"""games.jsonl の棋譜を盤面表示する。  python3 bench/show.py <games.jsonl> [game_id ...] [--upto N]"""
import json
import sys

N = 15


def render(rec, upto=None):
    moves = [tuple(m) for m in rec["opening"]] + [tuple(m) for m in rec["moves"]]
    if upto is not None:
        moves = moves[:upto]
    grid = [["." for _ in range(N)] for _ in range(N)]
    for i, (x, y) in enumerate(moves):
        grid[y][x] = "X" if i % 2 == 0 else "O"
    if moves:
        x, y = moves[-1]
        grid[y][x] = grid[y][x].lower()
    head = "game %s: %s(X,black) vs %s(O,white) -> %s by %s, %d moves (opening %d)" % (
        rec["id"], rec["black"], rec["white"], rec.get("winner"), rec.get("reason"), len(moves), len(rec["opening"]))
    lines = [head, "   " + " ".join("%x" % i for i in range(N))]
    for y in range(N):
        lines.append("%2d " % y + " ".join(grid[y]))
    lines.append("moves: " + " ".join("%d,%d" % m for m in moves))
    return "\n".join(lines)


def main():
    args = sys.argv[1:]
    upto = None
    if "--upto" in args:
        i = args.index("--upto")
        upto = int(args[i + 1])
        del args[i:i + 2]
    path, ids = args[0], set(int(a) for a in args[1:])
    for line in open(path):
        rec = json.loads(line)
        if not ids or rec["id"] in ids:
            print(render(rec, upto))
            print()


if __name__ == "__main__":
    main()
