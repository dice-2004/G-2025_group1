# G2 — 新エンジン

授業版 (`gomoku_ai.c` + `logics/`) に代わる新しい五目並べエンジン。計画は [docs/ROADMAP.md](../docs/ROADMAP.md)。

## ビルドと実行

```bash
make -C engine          # engine/build/pbrain-g2 (Gomocup エンジン), engine/build/g2-test
make -C engine test     # 差分更新の自己検証 + Python レフェリーとの禁じ手照合
make g2class            # リポジトリ直下: 授業の枠組みに G2 を組み込んだ ./gomoku_g2
python3 bench/match.py g2 legacy --rule renju    # 対局 (bench/README.md)
```

## 構成

| ファイル | 内容 |
|---|---|
| `src/types.h` | 盤の定数 (最大 20 路、番兵 5 マス)、座標変換、ルール構造体 |
| `src/rule.c` | Gomocup のルール番号 (0/1/4) と授業審判の `check` (1..7) から `Rule` を作る |
| `src/pattern.c/.h` | 線パターン表。中心 ±5 マスの並び (20 ビット) → 五/達四/四/活三/… を色別に表引き。ルールが変わったら作り直す (約 0.12 秒) |
| `src/board.c/.h` | 盤面。着手/戻しで周囲 40 マスのキーとパターンを差分更新。Zobrist ハッシュ。連珠の禁じ手判定 (偽の三三を再帰で除外) |
| `src/think.c/.h` | 指し手選択 (暫定): 五 → 五止め → 達四 → 相手の活三への対応 → パターン点の 1 手読み |
| `src/gomocup.c` | Gomocup プロトコル (START/RESTART/INFO/BEGIN/TURN/BOARD/TAKEBACK/ABOUT/END) |
| `src/class_entry.c` | 授業の `calcPutPos` 入口 |
| `tests/g2_test.c` | `selftest` (差分更新 = 作り直し の検証)、`forbid` (禁じ手の一覧出力)、`bench` (速度) |
| `tests/compare_forbidden.py` | 禁じ手判定を `bench/rules.py` と照合 |

## パターンの定義

マス p に色 c を置いたとき、1 方向の線について (p を含む五だけを数える):

- `F5` 五 (ルールにより「ちょうど五」か「五以上」)
- `OL` 長連 (黒の長連が禁じ手のとき)
- `F4` 達四: 五にする点が 2 つ、5 マス離れている (`_XXXX_`)
- `D4` 一直線の四四: 五にする点が 2 つ以上だが達四ではない (`X.XXX.X` の中央など)
- `B4` 四: 五にする点が 1 つ
- `F3`/`B3` 活三/止め三: 次の 1 手で `F4`/`B4` にできる (連珠の黒は `D4`・`OL` になる手を除く)
- `F2`/`B2`/`F1`/`B1` 同様に 1 段ずつ

4 方向を合わせた `Pattern4` は FIVE > FLEX4 (達四・四四) > BLOCK4_FLEX3 (四三) > … の順。

## 検証状況 (2026-10-22)

- 差分更新: ランダム 300 局 x 着手/戻しで作り直しと完全一致 (連珠・自由・標準)
- 禁じ手: ランダム 2000 局面・約 39 万点で Python 版と不一致 0
- 速度: 着手+戻し 約 50 万回/秒 (最適化前)
