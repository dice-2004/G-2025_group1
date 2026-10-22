#!/usr/bin/env python3
"""局面を G2 (など Gomocup エンジン) に考えさせて、探索ログ (INFO verbose 1 の stderr) を表示する。

  python3 engine/tests/analyze.py <games.jsonl> <game_id> <手数> [--engine path] [--tc ms] [--rule 4]
  python3 engine/tests/analyze.py --moves "7,7 8,8 ..." [--engine path]

手数 N = 開局を含めて最初の N 手を置いた局面 (次は N+1 手目)。
"""
import argparse, json, subprocess, os

HERE = os.path.dirname(os.path.abspath(__file__))
ap = argparse.ArgumentParser()
ap.add_argument("games", nargs="?")
ap.add_argument("game_id", nargs="?", type=int)
ap.add_argument("ply", nargs="?", type=int)
ap.add_argument("--moves")
ap.add_argument("--engine", default=os.path.join(HERE, "..", "build", "pbrain-g2"))
ap.add_argument("--tc", type=int, default=1000)
ap.add_argument("--rule", type=int, default=4)
ap.add_argument("--extra", default="", help="追加の INFO (例: 'max_depth 6')")
a = ap.parse_args()

if a.moves:
    moves = [tuple(map(int, m.split(","))) for m in a.moves.split()]
else:
    for line in open(a.games):
        g = json.loads(line)
        if g["id"] == a.game_id:
            break
    else:
        raise SystemExit("game not found")
    moves = [tuple(m) for m in g["opening"]] + [tuple(m) for m in g["moves"]]
    moves = moves[: a.ply]
n = len(moves)
cmds = ["START 15", "INFO rule %d" % a.rule, "INFO timeout_turn %d" % a.tc, "INFO verbose 1"]
if a.extra:
    cmds.append("INFO " + a.extra)
cmds.append("BOARD")
for i, (x, y) in enumerate(moves):
    own = (i % 2) == (n % 2)
    cmds.append("%d,%d,%d" % (x, y, 1 if own else 2))
cmds += ["DONE", "END"]
r = subprocess.run([a.engine], input="\n".join(cmds) + "\n", capture_output=True, text=True)
print(r.stderr, end="")
print("reply:", r.stdout.split()[-1] if r.stdout.split() else "")
