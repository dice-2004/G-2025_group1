#!/usr/bin/env python3
"""対局の中で G2 が詰み (VCT) を見落とした局面を集める (詰みの見落としの問題集づくり)。

  python3 engine/tests/mate_miss.py games.jsonl [...] [--name g2] [--nodes 2000000] [--ms 3000] [-j 4] --out miss.tsv

G2 の各手番 P (G2 が打つ前) と、G2 が打った後の局面 Q について、大きな予算の df-pn (g2-tss dfpn) で調べる:
  own  : P で G2 に VCT がある
  opp  : Q で相手に VCT がある (G2 が打った手の後、相手が勝ちを持っている)
分類 (1 局につき最初の 1 回だけ数える):
  miss-win  : P で G2 に VCT があるのに、次の G2 の手番で VCT も VCF もない (勝ちを逃した)
  walk-in   : 打つ前の相手の手番 (前の Q) では相手に VCT がなく、P で G2 が打った後 (Q) に相手の VCT ができた。
              さらに P に「打った後に相手の VCT がない手」(安全な手) が 1 つ以上あれば walk-in-safe、なければ walk-in-forced
出力 (TSV): 種類  手順(P)  G2 の手  正解の手 (勝ちの初手 / 安全な手を ; 区切り)  df-pn のノード数  対局#手数  勝敗
1〜3 列は defense_test.py の問題集と同じ形。
"""
import argparse, json, os, subprocess, sys
from multiprocessing import Pool

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "bench"))
from rules import renju_forbidden_reason  # noqa: E402

TSS = os.environ.get("G2_TSS", os.path.join(HERE, "..", "build", "g2-tss"))

ap = argparse.ArgumentParser()
ap.add_argument("games", nargs="+")
ap.add_argument("--name", default="g2", help="G2 の名前の先頭 (black / white の値)")
ap.add_argument("--nodes", type=int, default=2000000)
ap.add_argument("--ms", type=int, default=3000)
ap.add_argument("--safe-nodes", type=int, default=300000, help="安全な手を探すときの 1 手あたりの上限")
ap.add_argument("--cache", default=None)
ap.add_argument("--all", action="store_true", help="勝った対局も含める (既定は負けと引き分けだけ)")
ap.add_argument("-j", type=int, default=4)
ap.add_argument("--out", default="-")
a = ap.parse_args()
if a.cache is None:
    a.cache = os.path.join(HERE, "..", "cache", "mate_miss_cache.tsv")


def run_tss(args, lines):
    p = subprocess.run([TSS] + args, input="\n".join(lines) + "\n", capture_output=True, text=True)
    return p.stdout.splitlines()


def job(chunk):
    kind, lines = chunk
    if kind == "dfpn":
        return run_tss(["dfpn", "renju", str(a.nodes), str(a.ms)], lines)
    return run_tss(["dfpn", "renju", str(a.safe_nodes), "0"], lines)


cache = {}
if os.path.exists(a.cache):
    for l in open(a.cache):
        p = l.rstrip("\n").split("\t")
        if len(p) == 3:
            cache[(p[0], p[1])] = p[2]


def solve(kind, positions):
    """kind: 'dfpn' (大きな予算) / 'safe' (安全な手の判定用)。結果を cache に入れる"""
    todo = [p for p in dict.fromkeys(positions) if (kind, p) not in cache]
    if not todo:
        return
    print("%s: %d positions" % (kind, len(todo)), file=sys.stderr)
    step = max(1, min(50, len(todo) // (a.j * 4) + 1))
    chunks = [(kind, todo[i:i + step]) for i in range(0, len(todo), step)]
    os.makedirs(os.path.dirname(a.cache), exist_ok=True)
    with Pool(a.j) as pool, open(a.cache, "a") as f:
        for (k, ch), out in zip(chunks, pool.imap(job, chunks)):
            for p, r in zip(ch, out):
                cache[(k, p)] = r
                f.write("%s\t%s\t%s\n" % (k, p, r))
            f.flush()


def win_of(r):
    if r is None or not r.startswith("win"):
        return None
    return r.split()[1]


def nodes_of(r):
    for t in r.split():
        if t.startswith("nodes="):
            return int(t[6:])
    return -1


games = []
for path in a.games:
    for l in open(path):
        g = json.loads(l)
        me = "black" if g["black"].startswith(a.name) else "white" if g["white"].startswith(a.name) else None
        if me is None or (not a.all and g.get("winner") == me):
            continue
        mv = ["%d,%d" % tuple(m) for m in g["opening"] + g["moves"]]
        games.append((g, me, mv, "%s#%d" % (os.path.basename(os.path.dirname(os.path.abspath(path))), g["id"])))
print("games: %d" % len(games), file=sys.stderr)

# 1) G2 の各手番の P と Q を大きな予算で調べる
pos = []
for g, me, mv, gid in games:
    start = max(1, len(g["opening"]))
    for i in range(start, len(mv)):
        if (i % 2 == 0) != (me == "black"):
            continue
        pos.append(" ".join(mv[:i]))
        pos.append(" ".join(mv[:i + 1]))
solve("dfpn", pos)


def near(mv):
    occ = set(mv)
    cells = set()
    board = [[0] * 15 for _ in range(15)]
    for i, m in enumerate(mv):
        x, y = map(int, m.split(","))
        board[y][x] = 1 if i % 2 == 0 else 2
    black = len(mv) % 2 == 0
    for m in mv:
        x, y = map(int, m.split(","))
        for dx in range(-2, 3):
            for dy in range(-2, 3):
                X, Y = x + dx, y + dy
                if 0 <= X < 15 and 0 <= Y < 15 and "%d,%d" % (X, Y) not in occ and not (black and renju_forbidden_reason(board, X, Y)):
                    cells.add("%d,%d" % (X, Y))
    return sorted(cells)


# 2) 分類
rows, walk = [], []
for g, me, mv, gid in games:
    start = max(1, len(g["opening"]))
    turns = [i for i in range(start, len(mv)) if (i % 2 == 0) == (me == "black")]
    res = "win" if g.get("winner") == me else "draw" if g.get("winner") in (None, "draw") else "loss"
    got_miss = got_walk = False
    prev_q_opp = False
    for k, i in enumerate(turns):
        P, Q = " ".join(mv[:i]), " ".join(mv[:i + 1])
        own = win_of(cache.get(("dfpn", P)))
        opp = win_of(cache.get(("dfpn", Q)))
        if own and not got_miss and k + 1 < len(turns):
            nxt = " ".join(mv[:turns[k + 1]])
            if not win_of(cache.get(("dfpn", nxt))):
                rows.append(["miss-win", P, mv[i], own, str(nodes_of(cache[("dfpn", P)])), "%s#%d" % (gid, i + 1), res])
                got_miss = True
        if opp and not prev_q_opp and not own and not got_walk:
            walk.append((P, mv[i], gid, i + 1, res, nodes_of(cache[("dfpn", Q)])))
            got_walk = True
        prev_q_opp = bool(opp)

# 3) walk-in の局面で安全な手を探す (打った後に相手に VCT がない手)
cand = {}
for P, m, gid, ply, res, nn in walk:
    mv = P.split()
    cand[P] = [c for c in near(mv) if c != m]
solve("safe", [P + " " + c for P in cand for c in cand[P]])
for P, m, gid, ply, res, nn in walk:
    # 打ち切り (aborted) は安全と数えない
    safe = [c for c in cand[P] if cache.get(("safe", P + " " + c), "").startswith("nowin") and "aborted" not in cache[("safe", P + " " + c)]]
    rows.append(["walk-in-safe" if safe else "walk-in-forced", P, m, ";".join(safe), str(nn), "%s#%d" % (gid, ply), res])

out = sys.stdout if a.out == "-" else open(a.out, "w")
for r in rows:
    out.write("\t".join(r) + "\n")
cnt = {}
for r in rows:
    cnt[(r[0], r[6])] = cnt.get((r[0], r[6]), 0) + 1
print("games %d" % len(games), file=sys.stderr)
for k in sorted(cnt):
    print("%-15s %-5s %d" % (k[0], k[1], cnt[k]), file=sys.stderr)
