#!/usr/bin/env python3
"""開局ブックの生成: Rapfi (古典評価、長い持ち時間) で序盤の局面を解析し、最善手の表を作る。

木の作り方 (ルールごと、G2 が黒の木と白の木):
  - G2 の手番の局面: Rapfi の最善手 1 つだけを指して先へ進む (この手がブックの手)
  - 相手の手番の局面: Rapfi の multi-PV 上位 K 手 (最善との差が --margin 以内、少なくとも 2 手) と、
    サイト AI の実戦 (docs/benchmarks/web/games) で相手が指した手を全部たどる
  - 8 通りの対称は正規化して同一視 (bookkey.py)。解析結果は analysis_<rule>.jsonl に貯めるので途中再開できる

  python3 engine/book/gen_book.py --rule renju -j 4
  python3 engine/book/gen_book.py --rule freestyle -j 4
  python3 engine/book/build_book.py          # book_*.txt -> engine/src/book_data.c

出力: engine/book/book_<rule>.txt (1 行 1 局面: 正規形の着手列 / 最善手 / 評価 / 深さ)
"""

import argparse
import concurrent.futures as cf
import glob
import json
import os
import queue
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
from bookkey import SIZE, C, canonical, canonical_moves, transform, inverse  # noqa: E402

RULE_CODE = {"freestyle": 0, "standard": 1, "renju": 4}
MATE = 20000


def parse_value(s):
    if s.startswith("+M"):
        return MATE - (int(s[2:]) if s[2:].isdigit() else 0)
    if s.startswith("-M"):
        return -(MATE - (int(s[2:]) if s[2:].isdigit() else 0))
    if "INF" in s:
        return MATE if not s.startswith("-") else -MATE
    try:
        return int(s)
    except ValueError:
        return 0


class Rapfi:
    """Rapfi を 1 プロセス起動して YXBOARD + YXNBEST で解析する。"""

    def __init__(self, exe, rule, memory_mb):
        self.proc = subprocess.Popen([exe], cwd=os.path.dirname(exe), stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=subprocess.DEVNULL, text=True, bufsize=1)
        self.q = queue.Queue()
        threading.Thread(target=self._read, daemon=True).start()
        self.send("START %d" % SIZE)
        self.wait_ok()
        for k, v in (("rule", RULE_CODE[rule]), ("max_memory", memory_mb << 20), ("thread_num", 1),
                     ("show_detail", 2), ("timeout_match", 100000000), ("time_left", 100000000)):
            self.send("INFO %s %s" % (k, v))

    def _read(self):
        for line in self.proc.stdout:
            self.q.put(line.strip())
        self.q.put(None)

    def send(self, s):
        self.proc.stdin.write(s + "\n")
        self.proc.stdin.flush()

    def wait_ok(self):
        while True:
            line = self.q.get(timeout=60)
            if line is None:
                raise RuntimeError("rapfi exited")
            if line.upper().startswith("OK"):
                return

    def analyze(self, moves, npv, ms):
        """moves の局面 (手番は石数から) を解析。[(move, eval, depth)] を評価順に返す (move は (x,y))。"""
        self.send("YXHASHCLEAR")
        self.send("INFO timeout_turn %d" % ms)
        self.send("YXBOARD")
        me = len(moves) % 2   # 手番側の石は 手順の偶奇がこれと同じもの
        for i, (x, y) in enumerate(moves):
            self.send("%d,%d,%d" % (x, y, 1 if i % 2 == me else 2))
        self.send("DONE")
        self.send("YXNBEST %d" % npv)
        latest = {}   # move -> (depth, eval)
        cur = {}
        final = None
        deadline = time.monotonic() + ms / 1000 * 3 + 30
        while True:
            line = self.q.get(timeout=max(1, deadline - time.monotonic()))
            if line is None:
                raise RuntimeError("rapfi exited")
            if line.startswith("INFO "):
                parts = line.split(None, 2)
                if len(parts) < 3:
                    continue
                k, v = parts[1], parts[2]
                if k == "PV" and v != "DONE":
                    cur = {}
                elif k == "DEPTH":
                    cur["depth"] = int(v)
                elif k == "EVAL":
                    cur["eval"] = parse_value(v)
                elif k == "BESTLINE":
                    toks = v.split()
                    if toks:
                        cur["move"] = tuple(int(t) for t in toks[0].split(","))
                elif k == "PV" and v == "DONE":
                    if "move" in cur and "depth" in cur:
                        latest[cur["move"]] = (cur["depth"], cur.get("eval", 0))
                continue
            if line.startswith(("MESSAGE", "DEBUG", "OK", "ERROR", "UNKNOWN")) or not line:
                continue
            try:
                final = tuple(int(t) for t in line.split(","))
                break
            except ValueError:
                continue
        if not latest:
            return [(final, 0, 0)]
        maxd = max(d for d, _ in latest.values())
        res = sorted(((m, e, d) for m, (d, e) in latest.items() if d >= maxd - 1), key=lambda r: -r[1])
        # Rapfi が最終的に選んだ手を先頭に
        res.sort(key=lambda r: 0 if r[0] == final else 1)
        if final not in latest:
            res.insert(0, (final, res[0][1] if res else 0, maxd))
        return res

    def close(self):
        try:
            self.send("END")
            self.proc.wait(timeout=3)
        except Exception:
            self.proc.kill()


def key_str(moves):
    return " ".join("%d,%d" % m for m in moves)


class Cache:
    def __init__(self, path):
        self.path = path
        self.data = {}
        if os.path.exists(path):
            for line in open(path):
                r = json.loads(line)
                self.data[(r["key"], r["npv"] == 1)] = r
        self.lock = threading.Lock()

    def get(self, key, single):
        """single=True: 1 PV の解析 (ブックの手用)、False: multi-PV の解析 (相手の候補用)"""
        return self.data.get((key, single))

    def put(self, rec):
        with self.lock:
            self.data[(rec["key"], rec["npv"] == 1)] = rec
            with open(self.path, "a") as f:
                f.write(json.dumps(rec) + "\n")


def site_lines(rule):
    """サイト AI との実戦の棋譜: [(相手の色 1/2, 着手列)]"""
    out = []
    for f in glob.glob(os.path.join(REPO, "docs/benchmarks/web/games/*.jsonl")):
        for line in open(f):
            try:
                g = json.loads(line)
            except ValueError:
                continue
            if g.get("rule") != rule:
                continue
            ec = g.get("engine_color")
            if ec not in ("black", "white"):
                continue
            out.append((2 if ec == "black" else 1, [tuple(m) for m in g["moves"]]))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rule", default="renju", choices=["freestyle", "renju"])
    ap.add_argument("-j", type=int, default=4)
    ap.add_argument("--rapfi", default=os.environ.get("RAPFI_DIR", os.path.join(REPO, "..", "rapfi-bin")) + "/pbrain-rapfi")
    ap.add_argument("--max-ply", type=int, default=8, help="この石数未満の局面にブックの手を入れる")
    ap.add_argument("--opp-k", default="3,8,6,6,4,4,3,3", help="相手の手番の局面で追う候補数 (石数ごと)")
    ap.add_argument("--margin", type=int, default=250, help="最善との評価差がこれ以内の候補だけ追う")
    ap.add_argument("--ms", default="30000,30000,30000,20000,15000,15000,10000,10000",
                    help="解析の持ち時間 (石数ごと, ms)")
    ap.add_argument("--memory", type=int, default=512, help="Rapfi 1 プロセスの置換表 MB")
    ap.add_argument("--out-dir", default=HERE, help="解析キャッシュとブックの出力先")
    ap.add_argument("--dry", action="store_true", help="解析せず、キャッシュにある分だけで木を数える")
    args = ap.parse_args()

    opp_k = [int(v) for v in args.opp_k.split(",")]
    ms_by_ply = [int(v) for v in args.ms.split(",")]
    cache = Cache(os.path.join(args.out_dir, "analysis_%s.jsonl" % args.rule))
    sites = site_lines(args.rule)

    pool = queue.Queue()
    engines = []
    if not args.dry:
        for _ in range(args.j):
            e = Rapfi(os.path.abspath(args.rapfi), args.rule, args.memory)
            engines.append(e)
            pool.put(e)

    def analyze(moves, npv):
        """正規形の着手列を解析 (キャッシュ優先)。npv=1 はブックの手用の 1 PV の解析 (multi-PV より深く読める)、
        npv>1 は相手の候補用。multi-PV の解析は PV 数が足りなければやり直す"""
        k = key_str(moves)
        rec = cache.get(k, npv == 1)
        if rec and rec["npv"] >= npv:
            return rec
        if args.dry:
            return rec
        e = pool.get()
        try:
            ply = len(moves)
            ms = ms_by_ply[min(ply, len(ms_by_ply) - 1)]
            t0 = time.time()
            res = e.analyze(moves, npv, ms)
            rec = {"key": k, "npv": npv, "ms": ms, "time": round(time.time() - t0, 1),
                   "pv": [[m[0], m[1], ev, d] for m, ev, d in res]}
            cache.put(rec)
            print("ply %d  %-40s -> %s (%d) npv %d  %.1fs" % (ply, k, "%d,%d" % res[0][0], res[0][1], npv,
                                                               rec["time"]), flush=True)
            return rec
        finally:
            pool.put(e)

    # 木の節: (正規形の着手列, G2 の色 1=黒/2=白)。同じ局面は 1 回だけ
    level = {}
    for g2 in (1, 2):
        level[(key_str([]), g2)] = []
    book = {}       # 正規形 key -> (move, eval, depth)
    tree = []       # (石数, G2 の色, 正規形 key): 木の全節 (検証用の開局に使う)
    seen = set()
    for ply in range(args.max_ply):
        if not level:
            break
        # この深さで解析が要る局面と必要な PV 数
        need = set()
        for (k, g2), moves in level.items():
            tomove = 1 if ply % 2 == 0 else 2
            if tomove != g2 and ply == args.max_ply - 1:
                continue    # 最後の深さの相手の候補は使わない
            need.add((k, 1 if tomove == g2 else max(2, opp_k[min(ply, len(opp_k) - 1)])))
        movesof = {k: m for (k, _), m in level.items()}
        with cf.ThreadPoolExecutor(max_workers=max(1, args.j)) as ex:
            futs = {(k, npv == 1): ex.submit(analyze, movesof[k], npv) for k, npv in sorted(need)}
            recs = {kk: f.result() for kk, f in futs.items()}
        for (k, g2) in level:
            tree.append((ply, g2, k))
        nxt = {}
        n_our = n_opp = 0
        for (k, g2), moves in level.items():
            tomove = 1 if ply % 2 == 0 else 2
            rec = recs.get((k, tomove == g2))
            if rec is None:
                continue
            pv = [((p[0], p[1]), p[2], p[3]) for p in rec["pv"]]
            if tomove == g2:
                n_our += 1
                book[k] = pv[0]
                children = [pv[0][0]]
            else:
                n_opp += 1
                kk = opp_k[min(ply, len(opp_k) - 1)]
                best = pv[0][1]
                children = [m for i, (m, ev, _) in enumerate(pv[:kk]) if i < 2 or ev >= best - args.margin]
                if ply == 0:
                    # 相手の初手: 天元のほか、天元の隣と斜め隣 (サイトの AI がずらして打つ場合)
                    children = [(C, C), (C, C - 1), (C - 1, C - 1)]
                # サイトの実戦で相手が指した手 (この局面に来た棋譜だけ)
                for opp, gm in sites:
                    if opp != tomove or len(gm) <= ply:
                        continue
                    cm, t = canonical_moves(gm[:ply])
                    if key_str(cm) == k:
                        children.append(transform(t, *gm[ply]))
            for m in dict.fromkeys(children):
                nm, _ = canonical_moves(moves + [m])
                nk = key_str(nm)
                if (nk, g2) in seen:
                    continue
                seen.add((nk, g2))
                nxt[(nk, g2)] = nm
        print("== ply %d: our %d opp %d -> next %d (book %d)" % (ply, n_our, n_opp, len(nxt), len(book)), flush=True)
        level = nxt

    for e in engines:
        e.close()

    out = os.path.join(args.out_dir, "book_%s.txt" % args.rule)
    with open(out, "w") as f:
        f.write("# G2 開局ブック (%s)。gen_book.py が生成。正規形の着手列 <TAB> 最善手 <TAB> 評価 <TAB> 深さ\n" % args.rule)
        for k in sorted(book, key=lambda s: (len(s.split()), s)):
            m, ev, d = book[k]
            f.write("%s\t%d,%d\t%d\t%d\n" % (k if k else "-", m[0], m[1], ev, d))
    print("wrote %s (%d entries)" % (out, len(book)))
    with open(os.path.join(args.out_dir, "tree_%s.txt" % args.rule), "w") as f:
        f.write("# 石数 <TAB> G2 の色 (1=黒 2=白) <TAB> 正規形の着手列\n")
        for ply, g2, k in tree:
            f.write("%d\t%d\t%s\n" % (ply, g2, k if k else "-"))


if __name__ == "__main__":
    main()
