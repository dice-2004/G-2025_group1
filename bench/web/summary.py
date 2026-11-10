#!/usr/bin/env python3
"""docs/benchmarks/web/games/*.jsonl を集計して Markdown の結果表を出す。
  python3 bench/web/summary.py [--dir <棋譜のフォルダ>] [--write docs/benchmarks/web/RESULTS.md]
  --dir: 既定は docs/benchmarks/web/games (再測定ごとに games_<日付>/ に分けて保存している)
"""
import glob
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GAMES = os.path.join(os.path.dirname(os.path.dirname(HERE)), "docs", "benchmarks", "web", "games")


def load(games_dir=GAMES):
    rows = {}
    for path in sorted(glob.glob(os.path.join(games_dir, "*.jsonl"))):
        for line in open(path):
            r = json.loads(line)
            if r.get("reason") in ("harness-error", "site-timeout", "engine-start-error"):
                key = (r.get("site"), r.get("level"), r.get("rule"))
                rows.setdefault(key, {"w": 0, "l": 0, "d": 0, "err": 0, "games": []})["err"] += 1
                continue
            key = (r["site"], r["level"], r["rule"])
            e = rows.setdefault(key, {"w": 0, "l": 0, "d": 0, "err": 0, "games": []})
            me = r["engine_color"]
            if r["winner"] == me:
                e["w"] += 1
            elif r["winner"] == "draw":
                e["d"] += 1
            else:
                e["l"] += 1
            e["games"].append(r)
    return rows


def table(rows):
    out = ["| サイト | 難易度 | ルール | G2 の勝-分-敗 | 黒番 | 白番 | 平均手数 | G2 の平均思考(秒) | 備考 |",
           "|---|---|---|---|---|---|---|---|---|"]
    for (site, level, rule), e in sorted(rows.items()):
        g = e["games"]
        n = len(g)
        if not n:
            continue
        avg_moves = sum(len(x["moves"]) for x in g) / n
        times = [t for x in g for t in x.get("times", []) if t > 0]
        avg_t = sum(times) / len(times) if times else 0
        note = []
        both = {x["engine_color"] for x in g}
        if len(both) == 2:
            note.append("先後両方")
        elif both:
            note.append("エンジンは" + ("黒" if "black" in both else "白"))
        if any(x.get("ref_check", "ok") != "ok" for x in g):
            note.append("審判不一致あり")
        if e["err"]:
            note.append("エラー %d 局を除外" % e["err"])
        by = []
        for c in ("black", "white"):
            gc = [x for x in g if x["engine_color"] == c]
            w = sum(x["winner"] == c for x in gc)
            d = sum(x["winner"] == "draw" for x in gc)
            by.append("%d-%d-%d" % (w, d, len(gc) - w - d) if gc else "-")
        out.append("| %s | %s | %s | %d-%d-%d | %s | %s | %.0f | %.2f | %s |" % (
            site, level, rule, e["w"], e["d"], e["l"], by[0], by[1], avg_moves, avg_t, " / ".join(note)))
    return "\n".join(out)


if __name__ == "__main__":
    gdir = sys.argv[sys.argv.index("--dir") + 1] if "--dir" in sys.argv else GAMES
    md = table(load(gdir))
    if "--write" in sys.argv:
        path = sys.argv[sys.argv.index("--write") + 1]
        with open(path, "w") as f:
            f.write(md + "\n")
    print(md)
