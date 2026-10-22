#!/usr/bin/env python3
"""複数エンジンの総当たりでレーティング表を作る (Bradley-Terry 最尤推定、引分=0.5勝)。

例:
  python3 bench/ladder.py random greedy legacy rapfi-n1k rapfi-n10k --rule renju --tc 1000 --openings 6
  python3 bench/ladder.py ... --pairs adjacent   # 隣り合う強さの組だけ対戦 (速い)

基準 (--anchor、既定は先頭のエンジン) を 0 として各エンジンの Elo を出す。
結果は bench/results/<日時>_ladder/ に games.jsonl, ratings.json, ratings.md。
"""

import argparse
import concurrent.futures as cf
import datetime as dt
import itertools
import json
import math
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import match  # noqa: E402


def fit_ratings(results, names, anchor, iters=2000):
    """results: [(name_black, name_white, score_for_black)] から Elo を推定。"""
    idx = {n: i for i, n in enumerate(names)}
    score = [[0.0] * len(names) for _ in names]
    games = [[0] * len(names) for _ in names]
    for a, b, s in results:
        i, j = idx[a], idx[b]
        score[i][j] += s
        score[j][i] += 1 - s
        games[i][j] += 1
        games[j][i] += 1
    # 全勝/全敗で発散しないよう、各組に 0.5 勝 0.5 敗の仮想対局を足す (弱い事前分布)
    for i in range(len(names)):
        for j in range(len(names)):
            if i != j and games[i][j]:
                score[i][j] += 0.5
                games[i][j] += 1
    r = [0.0] * len(names)
    for _ in range(iters):
        new = []
        for i in range(len(names)):
            w = sum(score[i])
            d = 0.0
            for j in range(len(names)):
                if games[i][j]:
                    d += games[i][j] / (1.0 + 10 ** ((r[j] - r[i]) / 400.0))
            if d <= 0 or w <= 0:
                new.append(r[i])
                continue
            new.append(r[i] + 400.0 * math.log10(w / d) * 0.5)
        r = new
        base = r[idx[anchor]]
        r = [v - base for v in r]
    return {n: round(r[idx[n]], 1) for n in names}, games, score


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("engines", nargs="+")
    ap.add_argument("--pairs", choices=["all", "adjacent"], default="all")
    ap.add_argument("--anchor")
    ap.add_argument("--rule", default="renju", choices=sorted(match.RULE_GOMOCUP_CODE))
    ap.add_argument("--check", type=int, default=7)
    ap.add_argument("--tc", type=int, default=1000)
    ap.add_argument("--time-factor", type=float, default=3.0)
    ap.add_argument("--time-margin", type=float, default=1.0)
    ap.add_argument("--openings", default="6")
    ap.add_argument("--opening-moves", type=int, default=3)
    ap.add_argument("--seed", type=int, default=2025)
    ap.add_argument("--concurrency", "-j", type=int, default=max(1, (os.cpu_count() or 2) // 2))
    ap.add_argument("--out")
    args = ap.parse_args()
    args.protocol_log = False

    reg = match.load_registry()
    engs = [match.resolve_engine(e, reg) for e in args.engines]
    names = [e["name"] for e in engs]
    anchor = args.anchor or names[0]
    openings = match.load_openings(args.openings, args.opening_moves, args.seed)
    if args.pairs == "all":
        pairs = list(itertools.combinations(range(len(engs)), 2))
    else:
        pairs = [(i, i + 1) for i in range(len(engs) - 1)]
    jobs = []
    for i, j in pairs:
        for op in openings:
            jobs.append((engs[i], engs[j], op))
            jobs.append((engs[j], engs[i], op))

    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    out = args.out or os.path.join(match.HERE, "results", stamp + "_ladder")
    os.makedirs(out, exist_ok=True)
    gf = open(os.path.join(out, "games.jsonl"), "w")
    results = []
    t0 = time.time()
    with cf.ThreadPoolExecutor(max_workers=args.concurrency) as ex:
        futs = [ex.submit(match.play_game, b, w, op, args, k) for k, (b, w, op) in enumerate(jobs)]
        for n, fut in enumerate(cf.as_completed(futs), 1):
            r = fut.result()
            gf.write(json.dumps(r, ensure_ascii=False) + "\n")
            gf.flush()
            s = {"black": 1.0, "white": 0.0}.get(r["winner"], 0.5)
            results.append((r["black"], r["white"], s))
            print("[%d/%d] %s(B) vs %s(W): %s by %s" % (n, len(jobs), r["black"], r["white"], r["winner"], r["reason"]), flush=True)
    gf.close()
    ratings, games, score = fit_ratings(results, names, anchor)
    order = sorted(names, key=lambda n: -ratings[n])
    lines = ["# Ladder (%s, tc %d ms, %d openings x2)" % (args.rule, args.tc, len(openings)), "",
             "| engine | Elo (vs %s) | games | score |" % anchor, "|---|---:|---:|---:|"]
    idx = {n: i for i, n in enumerate(names)}
    for n in order:
        i = idx[n]
        g = sum(1 for a, b, _ in results if n in (a, b))
        sc = sum(s if a == n else 1 - s for a, b, s in results if n in (a, b))
        lines.append("| %s | %+.0f | %d | %.1f |" % (n, ratings[n], g, sc))
    lines += ["", "対戦表 (行が列に対して得た点 / 局数):", "",
              "| | " + " | ".join(order) + " |", "|---" * (len(order) + 1) + "|"]
    raw = {}
    for a, b, s in results:
        raw.setdefault((a, b), [0.0, 0])
        raw.setdefault((b, a), [0.0, 0])
        raw[(a, b)][0] += s
        raw[(a, b)][1] += 1
        raw[(b, a)][0] += 1 - s
        raw[(b, a)][1] += 1
    for a in order:
        cells = []
        for b in order:
            v = raw.get((a, b))
            cells.append("-" if a == b or not v else "%.1f/%d" % (v[0], v[1]))
        lines.append("| %s | %s |" % (a, " | ".join(cells)))
    lines.append("")
    lines.append("wall: %.0f s" % (time.time() - t0))
    md = "\n".join(lines) + "\n"
    with open(os.path.join(out, "ratings.md"), "w") as f:
        f.write(md)
    with open(os.path.join(out, "ratings.json"), "w") as f:
        json.dump({"ratings": ratings, "args": vars(args)}, f, ensure_ascii=False, indent=1)
    print(md)
    print("results:", out)


if __name__ == "__main__":
    main()
