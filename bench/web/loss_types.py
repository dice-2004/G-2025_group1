#!/usr/bin/env python3
"""G2 が負けた対局で、相手 (勝った側) の攻め方を分類する。

  python3 bench/web/loss_types.py docs/benchmarks/web/games_2026-11-10/*.jsonl [--drops drops.tsv] [--name g2]

相手の各手を、打った直後の形で 3 つに分ける (盤 15 路、禁じ手は考えない簡易判定):
  F = 四 (次に五を作れる)、T = 活三 (次に活四を作れる)、
  d = 受け (直前の G2 の手が四・活三で、自分は四・三を作らずに受けた)、q = 静かな手 (どれでもない)
数える範囲は「G2 が形勢を損ねた手」の直後から終局まで。--drops に engine/tests/drops.py の出力を渡すと
対局ごとに最初の悪手 (Rapfi の評価が --min 以上下がった手) から数える。渡さなければ 7 手目以降を全部数える。

分類:
  quiet  = 最後の四・三の連続 (詰め) の前に、静かな手が 2 手以上ある (静かな手で形を作ってから詰める)
  direct = 静かな手が 1 手以下 (ほぼ四・三だけで押し切る = VCT/VCF)
  open   = 悪手が 12 手目以内 (序盤で形勢を損ねた) も別に数える
"""
import argparse
import json
import os
import sys

N = 15
DIRS = ((1, 0), (0, 1), (1, 1), (1, -1))


def at(b, x, y):
    return b.get((x, y), -1) if 0 <= x < N and 0 <= y < N else 9   # 9 = 盤外


def makes_five(b, x, y, c):
    for dx, dy in DIRS:
        n = 1
        for s in (1, -1):
            k = 1
            while at(b, x + s * k * dx, y + s * k * dy) == c:
                n += 1
                k += 1
        if n >= 5:
            return True
    return False


def has_four(b, c, x, y):
    """(x,y) の石を含む線で、空点 1 つに打てば五になるか"""
    for dx, dy in DIRS:
        for k in range(-4, 5):
            ex, ey = x + k * dx, y + k * dy
            if at(b, ex, ey) == -1:
                b[(ex, ey)] = c
                ok = makes_five(b, ex, ey, c)
                del b[(ex, ey)]
                if ok:
                    return True
    return False


def open_four_at(b, x, y, c):
    """(x,y) に打った石で、両端が空いた四 (.XXXX.) ができたか"""
    for dx, dy in DIRS:
        cnt, ends = 1, 0
        for s in (1, -1):
            k = 1
            while at(b, x + s * k * dx, y + s * k * dy) == c:
                cnt += 1
                k += 1
            if at(b, x + s * k * dx, y + s * k * dy) == -1:
                ends += 1
        if cnt == 4 and ends == 2:
            return True
    return False


def has_three(b, c, x, y):
    """(x,y) の石を含む線で、空点 1 つに打てば活四になるか (活三)"""
    for dx, dy in DIRS:
        for k in range(-4, 5):
            ex, ey = x + k * dx, y + k * dy
            if at(b, ex, ey) == -1:
                b[(ex, ey)] = c
                ok = open_four_at(b, ex, ey, c)
                del b[(ex, ey)]
                if ok:
                    return True
    return False


def kinds(moves, start, win_c):
    """moves[i] の手の種類 (F/T/q) を、start 以降で勝った側 (win_c: 0=黒 1=白) の手だけ返す"""
    b = {}
    out = []
    threat = False   # 直前の負けた側の手が四か活三 (勝った側はそれを受ける必要があった)
    for i, (x, y) in enumerate(moves):
        c = i % 2
        b[(x, y)] = c
        if c != win_c:
            threat = has_four(b, c, x, y) or has_three(b, c, x, y)
            continue
        if i < start:
            continue
        if makes_five(b, x, y, c):
            out.append("5")
        elif has_four(b, c, x, y):
            out.append("F")
        elif has_three(b, c, x, y):
            out.append("T")
        else:
            out.append("d" if threat else "q")
    return "".join(out)


def classify(s):
    body = s.rstrip("5")
    while body.endswith("d"):   # 終局直前の受け (サイトの判定待ち) は除く
        body = body[:-1]
    j = len(body)
    while j > 0 and body[j - 1] in "FT":
        j -= 1
    before = body[:j]
    return ("quiet" if before.count("q") >= 2 else "direct"), len(body) - j, before.count("q")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("games", nargs="+")
    ap.add_argument("--drops", default=None, help="engine/tests/drops.py の出力 (最後の列が <フォルダ>#<id>#<手数>)")
    ap.add_argument("--name", default="g2")
    ap.add_argument("--start", type=int, default=7, help="--drops が無い局は、この手数 (1 始まり) の相手の手から数える")
    a = ap.parse_args()
    first_drop = {}
    if a.drops:
        for line in open(a.drops):
            f = line.rstrip("\n").split("\t")
            if len(f) < 7:
                continue
            _, gid, ply = f[6].rsplit("#", 2)
            first_drop[int(gid)] = min(first_drop.get(int(gid), 999), int(ply))
    rows = []
    for path in a.games:
        for line in open(path):
            g = json.loads(line)
            me = g.get("engine_color")
            if g.get("winner") not in ("black", "white") or g["winner"] == me:
                continue
            mv = [tuple(m) for m in g["opening"] + g["moves"]]
            d = first_drop.get(g["id"])
            start = d if d is not None else a.start - 1   # 悪手 (1 始まり d 手目) の次の手 = 0 始まり d
            s = kinds(mv, start, 0 if g["winner"] == "black" else 1)
            typ, fin, nq = classify(s)
            rows.append((g.get("site"), g.get("level"), g["rule"], me, g["id"], len(mv), d, s, typ, fin, nq))
    print("site\tlevel\trule\tG2\tid\t手数\t最初の悪手\t相手の手 (F=四 T=三 q=静か)\t型\t詰めの長さ\t詰め前の静かな手")
    for r in rows:
        print("\t".join("-" if v is None else str(v) for v in r))
    tot = len(rows)
    if tot:
        q = sum(r[8] == "quiet" for r in rows)
        op = sum(r[6] is not None and r[6] <= 12 for r in rows)
        print("\n負け %d 局: 静かな手で形を作る型 %d (%.0f%%)、四・三で直行 %d、悪手が 12 手目以内 %d"
              % (tot, q, 100.0 * q / tot, tot - q, op), file=sys.stderr)


if __name__ == "__main__":
    main()
