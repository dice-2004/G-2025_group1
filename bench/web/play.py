#!/usr/bin/env python3
"""自作エンジン (Gomocup プロトコル) を Web サイト上の AI と対局させるハーネス (Playwright)。

例:
  python3 bench/web/play.py gonu --level champion --rule renju --games 2 --tc 5000
  python3 bench/web/play.py gonu --level pro --rule freestyle --games 4 --side both

サイトごとの違いは bench/web/sites/<site>.py のアダプタに閉じ込める (Site の docstring 参照)。
棋譜は docs/benchmarks/web/games/<site>_<level>_<rule>.jsonl に追記する (bench/show.py で表示可)。
負けた棋譜は engine/tests/analyze.py で局面を考えさせて原因を調べる。
"""

import argparse
import datetime as dt
import importlib
import json
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
BENCH = os.path.dirname(HERE)
REPO = os.path.dirname(BENCH)
sys.path.insert(0, BENCH)
sys.path.insert(0, HERE)
from engine import Engine, EngineError  # noqa: E402
from match import load_registry, resolve_engine  # noqa: E402
from rules import BLACK, WHITE, RULE_GOMOCUP_CODE, Referee  # noqa: E402

CHROME = os.environ.get("CHROME_PATH") or next(
    (p for p in ("/opt/pw-browsers/chromium-1194/chrome-linux/chrome",) if os.path.exists(p)), None)
OUT_DIR = os.path.join(REPO, "docs", "benchmarks", "web", "games")


def launch(pw, headless=True):
    kw = {"args": ["--no-sandbox", "--autoplay-policy=no-user-gesture-required"], "headless": headless}
    if CHROME:
        kw["executable_path"] = CHROME
    return pw.chromium.launch(**kw)


def play_game(site, edef, args, game_id, engine_color):
    """site.new_game() 済みを前提に 1 局指す。結果 dict を返す。"""
    rule = args.rule
    ref = Referee(rule, 7)  # サイトとは別にこちらでも判定する (食い違いの検出用)
    rec = {"id": game_id, "black": None, "white": None, "rule": rule, "tc": args.tc, "opening": [],
           "moves": [], "times": [], "site": args.site, "level": args.level,
           "engine": edef["name"], "engine_color": "black" if engine_color == BLACK else "white",
           "site_times": [], "date": dt.datetime.now(dt.timezone.utc).replace(tzinfo=None).isoformat(timespec="seconds") + "Z"}
    names = {engine_color: edef["name"], 3 - engine_color: "%s:%s" % (args.site, args.level)}
    rec["black"], rec["white"] = names[BLACK], names[WHITE]
    hard = args.tc * args.time_factor / 1000.0 + args.time_margin
    eng = Engine(edef["cmd"], cwd=edef.get("cwd"), name=edef["name"], pre_delay=edef.get("pre_delay", 0.0))
    info = dict(edef.get("info", {}))
    try:
        eng.start(15, args.tc, RULE_GOMOCUP_CODE[rule], info)
    except EngineError as ex:
        eng.close()
        rec.update(winner="none", reason="engine-start-error", error=str(ex))
        return rec

    moves = []
    initialized = False
    winner = reason = None
    try:
        while True:
            site_result = site.result()
            color = BLACK if len(moves) % 2 == 0 else WHITE
            if site_result:
                winner, reason = site_result, "site-end"
                break
            if color == engine_color:
                deadline = time.monotonic() + hard
                if not moves and args.first_random:
                    # サイト AI が決定的でも棋譜が変わるよう、黒の初手を中央付近からランダムに選ぶ
                    # (エンジンには次の手番で BOARD として知らせる)
                    import random
                    x, y = random.choice([(7 + dx, 7 + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)])
                    rec["times"].append(0.0)
                    ref.play(x, y)
                    moves.append((x, y))
                    site.place(x, y)
                    continue
                if not initialized:
                    initialized = True
                    if not moves:
                        x, y, t = eng.begin(deadline)
                    else:
                        stones = [(mx, my, 1 if i % 2 == (0 if color == BLACK else 1) else 2)
                                  for i, (mx, my) in enumerate(moves)]
                        x, y, t = eng.board(stones, deadline)
                else:
                    x, y, t = eng.turn(moves[-1][0], moves[-1][1], deadline)
                rec["times"].append(round(t, 3))
                done, w, why = ref.play(x, y)
                if done and why.startswith(("illegal", "forbidden")):
                    winner, reason = "white" if engine_color == BLACK else "black", "engine-" + why
                    moves.append((x, y))
                    break
                moves.append((x, y))
                site.place(x, y)
            else:
                t0 = time.monotonic()
                mv = site.wait_move(moves, timeout=args.site_timeout)
                rec["site_times"].append(round(time.monotonic() - t0, 3))
                if mv is None:
                    sr = site.result()
                    winner, reason = (sr, "site-end") if sr else ("none", "site-timeout")
                    break
                moves.append(mv)
                done, w, why = ref.play(*mv)
                if done and not site.result():
                    rec.setdefault("warnings", []).append("referee ended game (%s) but site did not" % why)
            if len(moves) >= 225:
                winner, reason = "draw", "full"
                break
    except EngineError as ex:
        winner, reason = ("white" if engine_color == BLACK else "black"), "engine-error"
        rec["error"] = str(ex)
    finally:
        eng.close()
    rec["moves"] = [list(m) for m in moves]
    rec["winner"] = winner
    rec["reason"] = reason
    rec["ref_check"] = "ok"
    # サイトの判定とこちらの判定が食い違う場合は記録する
    ref2 = Referee(rule, 7)
    last = None
    for m in moves:
        last = ref2.play(*m)
        if last[0]:
            break
    if last and last[0]:
        rw = {BLACK: "black", WHITE: "white"}.get(last[1], "draw")
        if rw != winner:
            rec["ref_check"] = "mismatch: referee %s/%s" % (rw, last[2])
    return rec


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("site")
    ap.add_argument("--level", default=None)
    ap.add_argument("--rule", default="freestyle", choices=["freestyle", "renju", "standard"])
    ap.add_argument("--engine", default="g2")
    ap.add_argument("--games", type=int, default=2)
    ap.add_argument("--side", default="black", choices=["black", "white", "both"],
                    help="エンジンの手番。both は交互 (サイトが対応している範囲)")
    ap.add_argument("--tc", type=int, default=5000, help="エンジンの 1 手の持ち時間 (ms)")
    ap.add_argument("--time-factor", type=float, default=3.0)
    ap.add_argument("--time-margin", type=float, default=5.0)
    ap.add_argument("--site-timeout", type=float, default=120.0, help="サイトの手を待つ上限 (秒)")
    ap.add_argument("--first-random", action="store_true",
                    help="エンジンが黒のとき初手を中央 3x3 からランダムに (サイト AI が決定的な場合の局面の分散用。renju 初手中央のサイトでは不可)")
    ap.add_argument("--site-threads", type=int, default=2, help="gomocalc の思考スレッド数")
    ap.add_argument("--headed", action="store_true")
    ap.add_argument("--out", default=None)
    ap.add_argument("--screenshots", action="store_true", help="終局時の画面を保存")
    args = ap.parse_args()

    mod = importlib.import_module("sites." + args.site)
    if args.level is None:
        args.level = mod.DEFAULT_LEVEL
    edef = resolve_engine(args.engine, load_registry())

    from playwright.sync_api import sync_playwright
    os.makedirs(OUT_DIR, exist_ok=True)
    out = args.out or os.path.join(OUT_DIR, "%s_%s_%s.jsonl" % (args.site, args.level, args.rule))
    score = {"win": 0, "loss": 0, "draw": 0, "other": 0}
    with sync_playwright() as pw:
        browser = launch(pw, not args.headed)
        ctx = browser.new_context(viewport={"width": 1280, "height": 1000}, locale="en-US")
        page = ctx.new_page()
        site = mod.Site(page, args)
        for g in range(args.games):
            if args.side == "both":
                ec = BLACK if g % 2 == 0 else WHITE
            else:
                ec = BLACK if args.side == "black" else WHITE
            gid = int(time.time())
            try:
                site.new_game(args.rule, args.level, "black" if ec == BLACK else "white")
                rec = play_game(site, edef, args, gid, ec)
            except Exception as ex:  # サイト側の不具合・画面変更
                import traceback
                traceback.print_exc()
                rec = {"id": gid, "black": "?", "white": "?", "site": args.site, "level": args.level, "rule": args.rule,
                       "opening": [], "moves": [], "winner": "none", "reason": "harness-error", "error": repr(ex)}
            if args.screenshots:
                try:
                    page.screenshot(path=os.path.join(os.path.dirname(out), "%s_%s.png" % (args.site, gid)))
                except Exception:
                    pass
            win = rec.get("winner")
            me = "black" if ec == BLACK else "white"
            if win == me:
                score["win"] += 1
            elif win in ("black", "white"):
                score["loss"] += 1
            elif win == "draw":
                score["draw"] += 1
            else:
                score["other"] += 1
            with open(out, "a") as f:
                f.write(json.dumps(rec, ensure_ascii=False) + "\n")
            print("game %d/%d  engine=%s  winner=%s (%s)  moves=%d  ref=%s  %s" % (
                g + 1, args.games, me, win, rec.get("reason"), len(rec["moves"]), rec.get("ref_check"), score), flush=True)
        browser.close()
    print("RESULT %s %s %s: %s" % (args.site, args.level, args.rule, json.dumps(score)))


if __name__ == "__main__":
    main()
