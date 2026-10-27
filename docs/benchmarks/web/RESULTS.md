# サイト AI との実戦結果

2026-10-22、G2 v0.2 (commit e8beb3f 時点の `engine/build/pbrain-g2`、1 手 3〜5 秒、1 スレッド) vs 各サイトの最高難易度。
ハーネスは [bench/web/](../../../bench/web/README.md)。棋譜 (負けた局を含む全局) は [games/](./games/)、
`python3 bench/show.py docs/benchmarks/web/games/<ファイル>.jsonl` で盤面表示、
`engine/tests/analyze.py` で局面を G2 に再検討させられる。結果表は `python3 bench/web/summary.py` で再生成。

勝-分-敗は G2 から見た成績。サイト AI の多くは決定的なので、同じ棋譜が繰り返されることがある (勝ち負けの標本数は
見かけより少ない)。サイトの AI はこちらの CPU 上で動くため (ブラウザ内の JS/WASM)、同時に複数走らせると双方とも弱くなる。

| サイト | 難易度 | ルール | G2 の勝-分-敗 | 平均手数 | G2 の平均思考(秒) | 備考 |
|---|---|---|---|---|---|---|
| gomocalc | 100 | freestyle | 1-0-7 | 38 | 0.05 | 先後両方 |
| gomocalc | 100 | renju | 0-0-8 | 38 | 0.05 | 先後両方 |
| gomocalc | 1500 | freestyle | 0-0-6 | 38 | 0.80 | 先後両方 |
| gomocalc | 1500 | renju | 0-0-6 | 40 | 0.72 | 先後両方 |
| gomocalc | 250 | freestyle | 0-0-8 | 36 | 0.12 | 先後両方 |
| gomocalc | 250 | renju | 0-0-8 | 48 | 0.14 | 先後両方 |
| gomocalc | 500 | freestyle | 1-0-5 | 38 | 0.29 | 先後両方 |
| gomocalc | 500 | renju | 0-0-6 | 37 | 0.26 | 先後両方 |
| gomocalc | 5000 | freestyle | 1-0-3 | 37 | 2.81 | 先後両方 |
| gomocalc | 5000 | renju | 0-0-6 | 47 | 2.66 | 先後両方 |
| gomokuonline | expert | freestyle | 4-0-0 | 20 | 1.72 | エンジンは黒 |
| gonu | champion | freestyle | 0-0-4 | 68 | 3.04 | エンジンは黒 |
| gonu | champion | renju | 0-0-3 | 61 | 2.98 | エンジンは黒 |
| gonu | normal | freestyle | 1-0-0 | 19 | 0.39 | エンジンは黒 |
| gonu | pro | freestyle | 6-0-2 | 52 | 1.80 | 先後両方 |
| gonu | pro | renju | 2-0-4 | 30 | 1.61 | 先後両方 |
| playgomokuonline | 10 | freestyle | 4-0-0 | 22 | 1.46 | 先後両方 |
| playgomokuonline | 10 | renju | 4-0-0 | 22 | 1.77 | 先後両方 |
| trygomoku | master | freestyle | 4-0-0 | 31 | 1.61 | 先後両方 |
| trygomoku | master | renju | 4-0-0 | 30 | 1.64 | 先後両方 |

## 読み取れること

- **T1 のサイト (trygomoku Master、gomokuonline.org Expert、playgomokuonline Lv10) には全勝** (20-0。1 手 1〜2 秒で
  20〜30 手以内に決着)。T1 は突破済み。
- **playgonu Pro (JS の minimax) には黒番なら全勝 (4-0)、白番だと負ける**: 連珠の白番 0-4、自由の白番 4-2。
  負ける局は同じ序盤から G2 の白が崩れる。
- **Rapfi NNUE (playgonu Champion = ブラウザ内 5 秒/手、gomocalc = 5 秒/手・2 スレッド) には勝てていない**:
  Champion 0-7 (G2 は黒、連珠・自由とも)、gomocalc 1-9 (勝ったのは自由・黒番の 1 局のみ。連珠は黒番 0-3・白番 0-3)。
  ただし黒番の自由ルールでは 57〜75 手まで粘る局が多く、完敗ではない。連珠は黒の禁じ手と受けが絡んで不利が大きい。
- **G2 の白番が特に弱い**: gomocalc 白番 0-5 (自由 0-2、連珠 0-3)、Pro 白番 4-6。黒 7,7 に対する G2 白の 2 手目が 5,5 (2 マス離れた斜め) で、
  G2 自身の評価も -700〜-1700 と悪い。序盤で受け・手の選び方を見直す価値が大きい (開局 DB や白番専用の定石、
  近接した応手の優先)。
- 時間の使い方: G2 は `tc=5000` でも平均 2.7〜3.0 秒しか使わない (最大 4.7 秒)。サイトの Champion は平均 3.3 秒、最大 5 秒。
  持ち時間を使い切る方向 (反復深化の打ち切り判断の見直し) で強さが上がる余地がある。

## gomocalc 持ち時間ラダー (2026-10-27)

gomocalc (Rapfi NNUE、2 スレッド) の 1 手の持ち時間を変え、G2 も同じ持ち時間で先後交互に指した。G2 の勝-分-敗:

| 1 手の時間 | 自由 | 連珠 |
|---|---|---|
| 100 ms | 1-0-7 | 0-0-8 |
| 250 ms | 0-0-8 | 0-0-8 |
| 500 ms | 1-0-5 | 0-0-6 |
| 1500 ms | 0-0-6 | 0-0-6 |
| 5000 ms | 1-0-3 | 0-0-6 |

全設定の合計でも 3-0-63 (最短の 100 ms でも 1-0-15)、**G2 v0.2 は gomocalc の最弱設定にも届かない**。物差しとしてはさらに弱い設定
(最大ノード数 `maxNodes` や最大深さ `maxDepth` の制限) が要る。`sites/gomocalc.py` の `_set("maxNodes", ...)` / `maxDepth` で指定できる。

## 再現コマンド

```bash
make -C engine
python3 bench/web/play.py gonu --level champion --rule renju --games 3 --tc 5000
python3 bench/web/play.py gomocalc --level 5000 --rule freestyle --games 4 --side both --tc 5000
python3 bench/web/play.py trygomoku --level master --rule renju --games 4 --side both --tc 3000
```
