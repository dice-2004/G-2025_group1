#!/usr/bin/env python3
"""負けた対局で、G2 が「負けの決まる手」を打った局面を Rapfi で探す (受けの弱点の洗い出し)。

  python3 engine/tests/blunders.py docs/benchmarks/web/games/gomocalc_500_renju.jsonl [--side white] [--nodes 300000]

G2 の各手について、打つ前 (G2 の手番) と打った後 (相手の手番) の局面を Rapfi で評価し、
打つ前は詰みで負けていないのに、打った後は相手に詰み (+M) がある手を「負けを決めた手」として出す。
出力: 対局, 手数, G2 の手, Rapfi の手, 打つ前の評価, 打った後の評価 (相手から)。
"""
import argparse, json, os, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ap = argparse.ArgumentParser()
ap.add_argument("games")
ap.add_argument("--side", default="white", help="G2 が black / white / any の対局")
ap.add_argument("--name", default="g2", help="G2 の名前 (black/white の値の先頭)")
ap.add_argument("--nodes", type=int, default=300000)
ap.add_argument("--rule", default=None)
a = ap.parse_args()

games = []
for l in open(a.games):
    g = json.loads(l)
    me = "black" if g["black"].startswith(a.name) else "white" if g["white"].startswith(a.name) else None
    if me is None or g.get("winner") in (None, me, "draw") or (a.side != "any" and me != a.side):
        continue
    games.append((g, me))
print("lost games:", len(games), file=sys.stderr)

pos, meta = [], []
for gi, (g, me) in enumerate(games):
    mv = ["%d,%d" % tuple(m) for m in g["opening"] + g["moves"]]
    for i, m in enumerate(mv):
        if (i % 2 == 0) != (me == "black") or i + 1 >= len(mv):
            continue
        pos.append(" ".join(mv[:i]))
        pos.append(" ".join(mv[:i + 1]))
        meta.append((gi, i + 1, m))
rule = a.rule or games[0][0]["rule"] if games else "renju"
with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
    f.write("\n".join(pos) + "\n")
out = subprocess.run([sys.executable, os.path.join(HERE, "rapfi_label.py"), "--rule", rule, "--nodes", str(a.nodes), "-j", "2", f.name],
                     capture_output=True, text=True, check=True).stdout.splitlines()

print("game\tply\tg2_move\trapfi_move\tbefore(g2)\tafter(opp)")
for k, (gi, ply, m) in enumerate(meta):
    b, af = out[2 * k].split(), out[2 * k + 1].split()
    if not b or not af:
        continue
    before, rmove, after = b[0], b[1] if len(b) > 1 else "?", af[0]
    if not before.startswith("-M") and (after.startswith("+M") or after.startswith("M")):
        print("%d\t%d\t%s\t%s\t%s\t%s" % (games[gi][0]["id"], ply, m, rmove, before, after))
