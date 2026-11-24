#!/usr/bin/env python3
"""mate_miss.py の出力 (詰みの見落としの問題集) を G2 に解かせて、見落としを避けられる割合を数える。

  python3 engine/tests/mate_puzzle.py miss.tsv [--kinds walk-in-safe] [--engine path] [--tc 300] [--info p_x=1 ...] [-j 4]

walk-in-safe: G2 の手が「安全な手」(4 列目) なら正解。負けの手 (3 列目) なら不正解。それ以外は打った後の局面を
  g2-tss dfpn (相手が攻め側、--nodes) で調べ、相手に VCT がなければ正解。
miss-win: 打った後に「相手がパスしたら G2 に VCT」かつ、相手の受け (石の近くの全部の手) のどれの後にも G2 に VCT があれば正解。
同じ局面は 1 回だけ数える。
"""
import argparse, os, subprocess, sys, time
from multiprocessing import Pool

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "bench"))
from engine import Engine  # noqa: E402
from rules import RULE_GOMOCUP_CODE, renju_forbidden_reason  # noqa: E402

TSS = os.environ.get("G2_TSS", os.path.join(HERE, "..", "build", "g2-tss"))
ap = argparse.ArgumentParser()
ap.add_argument("miss")
ap.add_argument("--kinds", default="walk-in-safe,miss-win")
ap.add_argument("--engine", default=os.path.join(HERE, "..", "build", "pbrain-g2"))
ap.add_argument("--tc", type=int, default=300)
ap.add_argument("--info", nargs="*", default=[])
ap.add_argument("--nodes", type=int, default=1000000)
ap.add_argument("-j", type=int, default=4)
ap.add_argument("-v", action="store_true")
a = ap.parse_args()


def think(line):
    moves = [tuple(map(int, m.split(","))) for m in line.split()]
    eng = Engine([os.path.abspath(a.engine)], name="g2")
    try:
        info = dict(kv.split("=", 1) for kv in a.info)
        eng.start(15, a.tc, RULE_GOMOCUP_CODE["renju"], info)
        n = len(moves)
        stones = [(x, y, 1 if (i % 2) == (n % 2) else 2) for i, (x, y) in enumerate(moves)]
        t0 = time.monotonic()
        x, y, _ = eng.board(stones, time.monotonic() + a.tc / 1000 * 3 + 5)
        return "%d,%d" % (x, y), time.monotonic() - t0
    except Exception as e:  # noqa: BLE001
        return "error:%s" % e, 0
    finally:
        eng.close()


def dfpn(lines, nodes):
    if not lines:
        return []
    return subprocess.run([TSS, "dfpn", "renju", str(nodes), "0"], input="\n".join(lines) + "\n",
                          capture_output=True, text=True).stdout.splitlines()


def near(mv):
    occ = set(mv)
    board = [[0] * 15 for _ in range(15)]
    for i, m in enumerate(mv):
        x, y = map(int, m.split(","))
        board[y][x] = 1 if i % 2 == 0 else 2
    black = len(mv) % 2 == 0
    cells = set()
    for m in mv:
        x, y = map(int, m.split(","))
        for dx in range(-2, 3):
            for dy in range(-2, 3):
                X, Y = x + dx, y + dy
                c = "%d,%d" % (X, Y)
                if 0 <= X < 15 and 0 <= Y < 15 and c not in occ and not (black and renju_forbidden_reason(board, X, Y)):
                    cells.add(c)
    return sorted(cells)


def job_check(arg):
    kind, P, ans = arg
    if kind == "walk-in-safe":
        r = dfpn([P + " " + ans], a.nodes)
        return r[0].startswith("nowin") and "aborted" not in r[0]
    Q = P + " " + ans
    rep = near(Q.split())
    out = dfpn([Q + " " + c for c in rep], a.nodes)
    return all(o.startswith("win") for o in out)


def main():
    kinds = a.kinds.split(",")
    seen, puz = set(), []
    for l in open(a.miss):
        r = l.rstrip("\n").split("\t")
        if r[0] in kinds and r[1] not in seen:
            seen.add(r[1])
            puz.append(r)
    with Pool(a.j) as pool:
        ans = pool.map(think, [p[1] for p in puz])
    checks, idx = [], []
    res = [None] * len(puz)
    for k, (p, (m, dt)) in enumerate(zip(puz, ans)):
        if m.startswith("error"):
            res[k] = False
        elif p[0] == "walk-in-safe" and m in p[3].split(";"):
            res[k] = True
        elif p[0] == "walk-in-safe" and m == p[2]:
            res[k] = False
        elif p[0] == "miss-win" and m == p[3]:
            res[k] = True
        else:
            checks.append((p[0], p[1], m))
            idx.append(k)
    with Pool(a.j) as pool:
        for k, ok in zip(idx, pool.map(job_check, checks)):
            res[k] = ok
    tot = {}
    for p, (m, dt), ok in zip(puz, ans, res):
        t = tot.setdefault(p[0], [0, 0, 0.0])
        t[0] += 1
        t[1] += bool(ok)
        t[2] += dt
        if a.v:
            print("%s\t%s\t%s\t%s\t%s" % (p[0], "ok" if ok else "NG", m, p[2], p[5]))
    for k, (n, ok, dt) in tot.items():
        print("%-13s %d/%d ok (avg %.0f ms)" % (k, ok, n, dt / n * 1000))


main()
