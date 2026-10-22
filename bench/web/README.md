# bench/web — Web サイト上の AI との実戦ハーネス

自作エンジン (Gomocup プロトコル、既定は G2 = `engine/build/pbrain-g2`) を、実在の五目並べサイトの AI と
ヘッドレス Chromium (Playwright) で自動対局させる。結果は `docs/benchmarks/web/` に残る。

## 準備

```bash
make -C engine                  # engine/build/pbrain-g2
pip install playwright          # ブラウザ本体は同梱の Chromium を使う (playwright install は不要)
# Chromium の場所は環境変数 CHROME_PATH、無ければ /opt/pw-browsers/chromium-*/chrome-linux/chrome
```

クラウド環境のネットワーク許可に各サイトのドメインが要る (playgonu.com, gomocalc.com, trygomoku.com, gomokuonline.org,
playgomokuonline.com)。2026-10-22 時点で全部到達できる。

## 使い方

```bash
# 例: playgonu の Champion (Rapfi NNUE 5秒/手) に連珠で 3 局 (Champion は白固定)
python3 bench/web/play.py gonu --level champion --rule renju --games 3 --tc 5000
# 例: gomocalc (Rapfi NNUE) を 1 手 5 秒、先後交互
python3 bench/web/play.py gomocalc --level 5000 --rule freestyle --games 4 --side both --tc 5000
# 結果表
python3 bench/web/summary.py --write docs/benchmarks/web/RESULTS.md
```

主なオプション: `--engine` (bench/engines.json の名前、`g2+p_xxx=値` も可)、`--tc` (エンジンの 1 手ms)、
`--side black|white|both`、`--first-random` (エンジンが黒のとき初手を中央 3x3 からランダムに。サイト AI が決定的で
同じ棋譜になるのを避ける。連珠で初手中央固定のサイトでは使わない)、`--screenshots`、`--headed`。
**並列に走らせるときは CPU を食い合う (サイトの WASM/JS AI もこちらの CPU で動く) ので 2 並列までにする。**

棋譜は `docs/benchmarks/web/games/<site>_<level>_<rule>.jsonl` に追記 (`bench/show.py` で盤面表示、
`engine/tests/analyze.py` で局面を再検討できる `bench/match.py` と同じ形式)。独自項目: `site`, `level`,
`engine_color`, `times` (エンジンの 1 手の秒)、`site_times` (サイトが返すまでの秒)、`ref_check` (自前の審判と
サイトの勝敗判定の一致。`mismatch` なら要調査)。

## サイトごとのアダプタ (`sites/`)

`play.py` はサイトに依存しない。アダプタは次の 5 つを持つ `Site` クラス。

| メソッド | 役割 |
|---|---|
| `new_game(rule, level, engine_color)` | 設定して新しい 1 局を始める (サイト AI が先手ならそこで打ち始める) |
| `place(x, y)` | こちらの手を盤に打つ (x=列, y=上からの行) |
| `wait_move(moves, timeout)` | サイト AI の次の手 `(x, y)`。終局なら None |
| `result()` | `None` / `black` / `white` / `draw` |
| `stones()` | 盤の石 (デバッグ用) |

| アダプタ | 状態の読み方 | 打ち方 | 制約 |
|---|---|---|---|
| `gonu` (playgonu.com) | SVG の circle | `.board-point` の click | Champion は人間=黒固定 (エンジンは黒)。Pro 以下は先後可 |
| `trygomoku` | グローバル変数 `board`/`gameOver`/`winner` | `placeStone(row,col)` | 先後可。freestyle/renju、盤 15 のみ実装 |
| `gomokuonline` | `window.app.game` | `app.handleCellClick` | AI は白固定。freestyle のみ |
| `playgomokuonline` | 画面の MOVE HISTORY 文字列 | canvas の座標クリック | 先後可。強さ 1..10、Renju はチェックボックス |
| `gomocalc` | Vuex ストア (`position.position`) | `position/makeMove` + 盤コンポーネントの `startThink()` | 先後可。強さは 1 手の持ち時間 (ms)、スレッド数 `--site-threads` |

サイトの画面が変わると壊れる。壊れたら `Site` の該当メソッドだけ直す。新サイトは `sites/<name>.py` を足すだけ。
