#!/usr/bin/env python3
"""エンジン同士の対局ハーネス (Gomocup プロトコル)。

例:
  # 現行AI vs 参照ボット、授業ルール(check=7)、1手500ms、20開局x先後入替=40局
  python3 bench/match.py legacy greedy --rule class --tc 500 --openings 20

  # 任意コマンドのエンジンを直接指定
  python3 bench/match.py "cmd:./bench/build/pbrain-legacy --data ." rapfi-d4 --rule renju

エンジン名は bench/engines.json に登録する。結果は bench/results/<日時>_<A>_vs_<B>/
に games.jsonl (棋譜) と summary.json / summary.md を出力する。
"""

import argparse
import concurrent.futures as cf
import datetime as dt
import json
import math
import os
import random
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from engine import Engine, EngineError  # noqa: E402
from rules import BLACK, WHITE, EMPTY, N, RULE_GOMOCUP_CODE, Referee  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)


def load_registry():
    path = os.path.join(HERE, "engines.json")
    with open(path) as f:
        return json.load(f)


def resolve_engine(spec, registry):
    """'name' か 'cmd:<コマンド>' からエンジン定義を返す。"""
    if spec.startswith("cmd:"):
        return {"name": os.path.basename(spec[4:].split()[0]), "cmd": spec[4:], "cwd": REPO}
    if spec not in registry:
        sys.exit("unknown engine '%s' (bench/engines.json: %s)" % (spec, ", ".join(sorted(registry))))
    e = dict(registry[spec])
    e["name"] = spec
    sub = {"repo": REPO, "bench": HERE, "rapfi": os.environ.get("RAPFI_DIR", os.path.join(REPO, "..", "rapfi-bin"))}
    e["cmd"] = e["cmd"].format(**sub)
    e["cwd"] = e.get("cwd", "{repo}").format(**sub)
    return e


# ---------------------------------------------------------------- openings
def parse_opening(text):
    moves = []
    for tok in text.replace(";", " ").split():
        x, y = tok.split(",")
        moves.append((int(x), int(y)))
    return moves


def random_openings(count, nmoves, seed, radius=3):
    """中央 (7,7) 周辺にランダムに nmoves 手置いた開局を作る (黒の初手は天元)。"""
    rng = random.Random(seed)
    seen = set()
    out = []
    c = N // 2
    while len(out) < count:
        moves = [(c, c)]
        used = {(c, c)}
        while len(moves) < nmoves:
            x = c + rng.randint(-radius, radius)
            y = c + rng.randint(-radius, radius)
            if (x, y) not in used:
                used.add((x, y))
                moves.append((x, y))
        key = tuple(moves)
        if key not in seen:
            seen.add(key)
            out.append(moves)
    return out


def load_openings(arg, nmoves, seed):
    if arg.isdigit():
        return random_openings(int(arg), nmoves, seed)
    out = []
    with open(arg) as f:
        for line in f:
            line = line.split("#")[0].strip()
            if line:
                out.append(parse_opening(line))
    return out


# ---------------------------------------------------------------- one game
def play_game(black, white, opening, args, game_id):
    """black/white: エンジン定義 dict。結果 dict を返す。"""
    ref = Referee(args.rule, args.check)
    log = []  # エラー時の調査用に常に取り、--protocol-log 時かエラー時だけ棋譜に残す
    engines = {}
    record = {"id": game_id, "black": black["name"], "white": white["name"],
              "rule": args.rule, "check": args.check, "tc": args.tc,
              "opening": [list(m) for m in opening], "moves": [], "times": []}
    hard = args.tc * args.time_factor / 1000.0 + args.time_margin

    def finish(winner, reason):
        record["winner"] = {BLACK: "black", WHITE: "white"}.get(winner, "draw")
        record["reason"] = reason
        if args.protocol_log or "error" in record:
            record["protocol"] = log[-60:] if not args.protocol_log else log
        return record

    try:
        for color, e in ((BLACK, black), (WHITE, white)):
            eng = Engine(e["cmd"], cwd=e.get("cwd"), name=e["name"] + ("/B" if color == BLACK else "/W"), log=log,
                         pre_delay=e.get("pre_delay", 0.0))
            engines[color] = eng
            eng.start(N, args.tc, RULE_GOMOCUP_CODE[args.rule], e.get("info"))
    except EngineError as ex:
        for eng in engines.values():
            eng.close()
        record["error"] = str(ex)
        return finish(EMPTY, "start-error")

    all_moves = []
    try:
        for x, y in opening:
            done, winner, reason = ref.play(x, y)
            all_moves.append((x, y))
            if done:
                return finish(winner, "opening-" + reason)
        record["moves"] = []
        initialized = {BLACK: False, WHITE: False}
        while True:
            color = ref.to_move()
            eng = engines[color]
            deadline = time.monotonic() + hard
            if not initialized[color]:
                initialized[color] = True
                if not all_moves:
                    x, y, t = eng.begin(deadline)
                else:
                    stones = []
                    for i, (mx, my) in enumerate(all_moves):
                        stone_color = BLACK if i % 2 == 0 else WHITE
                        stones.append((mx, my, 1 if stone_color == color else 2))
                    x, y, t = eng.board(stones, deadline)
            else:
                lx, ly = all_moves[-1]
                x, y, t = eng.turn(lx, ly, deadline)
            record["moves"].append([x, y])
            record["times"].append(round(t, 3))
            all_moves.append((x, y))
            done, winner, reason = ref.play(x, y)
            if done:
                return finish(winner, reason)
    except EngineError as ex:
        loser = ref.to_move()
        reason = "timeout" if "timeout" in str(ex) else "engine-error"
        record["error"] = str(ex)
        return finish(WHITE if loser == BLACK else BLACK, reason)
    finally:
        for eng in engines.values():
            eng.close()


# ---------------------------------------------------------------- stats
def elo_from_score(p):
    p = min(max(p, 1e-6), 1 - 1e-6)
    return -400.0 * math.log10(1.0 / p - 1.0)


def summarize(results, name_a, name_b):
    w = d = l = 0
    by_color = {"black": [0, 0, 0], "white": [0, 0, 0]}
    reasons = {}
    times = {name_a: [], name_b: []}
    for r in results:
        a_is_black = r["black"] == name_a
        a_color = "black" if a_is_black else "white"
        if r["winner"] == "draw":
            d += 1
            by_color[a_color][1] += 1
        elif r["winner"] == a_color:
            w += 1
            by_color[a_color][0] += 1
        else:
            l += 1
            by_color[a_color][2] += 1
        key = "%s:%s" % ("A" if r["winner"] == a_color else ("B" if r["winner"] != "draw" else "-"), r["reason"])
        reasons[key] = reasons.get(key, 0) + 1
        mv = r["moves"]
        tm = r["times"]
        # 開局後の最初の手番がどちらかで、各手の時間を振り分ける
        first = BLACK if len(r["opening"]) % 2 == 0 else WHITE
        for i, t in enumerate(tm):
            c = first if i % 2 == 0 else 3 - first
            eng = r["black"] if c == BLACK else r["white"]
            times[eng].append(t)
    n = w + d + l
    score = (w + 0.5 * d) / n if n else 0.0
    if n > 1:
        var = (w * (1 - score) ** 2 + d * (0.5 - score) ** 2 + l * (0 - score) ** 2) / n
        se = math.sqrt(var / n)
    else:
        se = 0.5
    elo = elo_from_score(score)
    lo, hi = elo_from_score(score - 1.96 * se), elo_from_score(score + 1.96 * se)
    los = 0.5 * (1 + math.erf((w - l) / math.sqrt(2.0 * (w + l)))) if (w + l) else 0.5

    def tstat(v):
        if not v:
            return None
        return {"avg": round(sum(v) / len(v), 3), "max": round(max(v), 3)}

    return {"A": name_a, "B": name_b, "games": n, "W": w, "D": d, "L": l,
            "score": round(score, 4), "elo": round(elo, 1), "elo_ci95": [round(lo, 1), round(hi, 1)],
            "los": round(los, 4), "A_as_black_WDL": by_color["black"], "A_as_white_WDL": by_color["white"],
            "reasons": dict(sorted(reasons.items(), key=lambda kv: -kv[1])),
            "time": {k: tstat(v) for k, v in times.items()}}


def summary_md(s, args):
    lines = [
        "# %s vs %s" % (s["A"], s["B"]),
        "",
        "- rule: `%s`%s, tc: %d ms/手, games: %d" % (args.rule, " check=%d" % args.check if args.rule == "class" else "", args.tc, s["games"]),
        "- A の成績: **%d勝 %d分 %d敗** (score %.3f)" % (s["W"], s["D"], s["L"], s["score"]),
        "- Elo 差 (A-B): **%+.0f** (95%%CI %+.0f 〜 %+.0f), LOS %.1f%%" % (s["elo"], s["elo_ci95"][0], s["elo_ci95"][1], 100 * s["los"]),
        "- A 黒番 W/D/L: %s, A 白番 W/D/L: %s" % (s["A_as_black_WDL"], s["A_as_white_WDL"]),
        "- 終局理由 (勝者:理由): %s" % ", ".join("%s=%d" % kv for kv in s["reasons"].items()),
        "- 思考時間: %s" % json.dumps(s["time"], ensure_ascii=False),
    ]
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("a", help="エンジンA (engines.json の名前 or cmd:<コマンド>)")
    ap.add_argument("b", help="エンジンB")
    ap.add_argument("--rule", default="class", choices=sorted(RULE_GOMOCUP_CODE))
    ap.add_argument("--check", type=int, default=7, help="class ルールの禁じ手設定 (1..7)")
    ap.add_argument("--tc", type=int, default=1000, help="1手の持ち時間 ms (INFO timeout_turn)")
    ap.add_argument("--time-factor", type=float, default=3.0, help="持ち時間の何倍で時間切れ負けにするか")
    ap.add_argument("--time-margin", type=float, default=1.0, help="時間切れ判定の追加猶予 (秒)")
    ap.add_argument("--openings", default="10", help="開局ファイル or ランダム開局の数 (各開局を先後入替で2局)")
    ap.add_argument("--opening-moves", type=int, default=3, help="ランダム開局の手数")
    ap.add_argument("--seed", type=int, default=2025)
    ap.add_argument("--no-swap", action="store_true", help="先後入替をしない (A が常に黒)")
    ap.add_argument("--concurrency", "-j", type=int, default=max(1, (os.cpu_count() or 2) // 2))
    ap.add_argument("--out", help="出力ディレクトリ")
    ap.add_argument("--protocol-log", action="store_true", help="プロトコルの送受信を棋譜に残す")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    reg = load_registry()
    ea, eb = resolve_engine(args.a, reg), resolve_engine(args.b, reg)
    if ea["name"] == eb["name"]:
        eb["name"] += "#2"
    openings = load_openings(args.openings, args.opening_moves, args.seed)

    jobs = []
    for i, op in enumerate(openings):
        jobs.append((ea, eb, op))
        if not args.no_swap:
            jobs.append((eb, ea, op))

    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    out = args.out or os.path.join(HERE, "results", "%s_%s_vs_%s" % (stamp, ea["name"], eb["name"]).replace("/", "-"))
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "config.json"), "w") as f:
        json.dump({"args": vars(args), "A": ea, "B": eb}, f, ensure_ascii=False, indent=1)

    results = []
    lock = threading.Lock()
    gf = open(os.path.join(out, "games.jsonl"), "w")
    t0 = time.time()
    with cf.ThreadPoolExecutor(max_workers=args.concurrency) as ex:
        futs = [ex.submit(play_game, b, w, op, args, i) for i, (b, w, op) in enumerate(jobs)]
        for fut in cf.as_completed(futs):
            r = fut.result()
            with lock:
                results.append(r)
                gf.write(json.dumps(r, ensure_ascii=False) + "\n")
                gf.flush()
                if not args.quiet:
                    s = summarize(results, ea["name"], eb["name"])
                    print("[%3d/%d] #%d %s(B) vs %s(W): %s by %s, %d moves | A %d-%d-%d elo %+.0f" % (
                        len(results), len(jobs), r["id"], r["black"], r["white"], r["winner"], r["reason"],
                        len(r["opening"]) + len(r["moves"]), s["W"], s["D"], s["L"], s["elo"]), flush=True)
    gf.close()
    s = summarize(results, ea["name"], eb["name"])
    s["wall_sec"] = round(time.time() - t0, 1)
    with open(os.path.join(out, "summary.json"), "w") as f:
        json.dump(s, f, ensure_ascii=False, indent=1)
    md = summary_md(s, args)
    with open(os.path.join(out, "summary.md"), "w") as f:
        f.write(md)
    print(md)
    print("results:", out)


if __name__ == "__main__":
    main()
