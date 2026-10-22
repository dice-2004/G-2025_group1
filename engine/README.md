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
| `src/vcf.c/.h` | VCF (四の連続) / VCT (四と活三の連続) 詰み探索。連珠の禁じ手を両側で考慮。置換表つき。API は `vcf_solve` / `vct_solve` / `tss_solve` (パス時の相手の詰み) / `vcf_defenses` (相手の VCF の受け) |
| `src/eval.c/.h` | 評価の表: 4 方向のパターンの組 (12^4) → 点数。盤面の評価は空点の点数の和 (Board.vsum を差分更新) の手番側 − 相手側。重みは `tests/tune_eval.py` で調整 |
| `src/search.c/.h` | 本探索: 反復深化 + PVS + aspiration、置換表、強制手、killer/history、LMR、ルートで VCF/VCT と相手 VCF への受け。調整用の値は `INFO p_<名前> <値>` (`search_set_param`) |
| `src/think.c/.h` | 持ち時間 (timeout_turn / time_left) から 1 手の上限と目安を決めて本探索を呼ぶ |
| `src/gomocup.c` | Gomocup プロトコル (START/RESTART/INFO/BEGIN/TURN/BOARD/TAKEBACK/ABOUT/END) |
| `src/class_entry.c` | 授業の `calcPutPos` 入口 |
| `tests/g2_test.c` | `selftest` (差分更新 = 作り直し の検証)、`forbid` (禁じ手の一覧出力)、`bench` (着手速度)、`speed` (固定深さの探索速度、`tests/speed_positions.txt`)、`features` (評価調整用の特徴量) |
| `tests/tune_eval.py` | 評価の重みを棋譜の勝敗に合わせる (Texel 法)。`python3 engine/tests/tune_eval.py bench/results/*/games.jsonl` |
| `tests/analyze.py` | 棋譜の局面をエンジンに考えさせて探索ログを見る |
| `tests/compare_forbidden.py` | 禁じ手判定を `bench/rules.py` と照合 |
| `tests/tss_test.c` | `g2-tss`: VCF/VCT を局面列に対して解く (`solve`/`opp`/`defend`)、問題集 (`puzzles`) |
| `tests/puzzles/tactics.txt` | 詰み問題集 (手作りの禁じ手問題 + 実戦から収集した 95 問)。`make test` で解く |
| `tests/tss_verify.py` | VCF の勝ち手順を別実装のレフェリー (`bench/rules.py`) で検証 |
| `tests/vcf_brute.py` | 遅いが単純な VCF 全探索 (C 版との照合用) |
| `tests/check_defenses.py` | `vcf_defenses` が相手の VCF を消す手を全部返すかを総当たりで確認 |
| `tests/rapfi_label.py` / `tss_compare.py` | Rapfi の詰み評価を正解ラベルにして VCT の再現率・矛盾を数える |
| `tests/tss_bench.sh` | VCF/VCT の速度と再現率 (連珠の終盤 1507 局面 `puzzles/renju_endgames.*`、正解は Rapfi の詰み評価) |

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
- 速度: 着手+戻し 約 158 万回/秒、探索 約 83 万ノード/秒 (2026-10-22、方向ごとの差分更新後)
