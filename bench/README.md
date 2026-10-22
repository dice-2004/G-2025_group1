# bench/ — 対局ハーネス・強さ測定ツール

エンジン同士を自動で対局させ、勝率と Elo 差を出すためのツール群です。
エンジンは **Gomocup (piskvork) プロトコル** で話すことを前提にしています。
授業の枠組み (`calcPutPos`) の AI は `pbrain_adapter.c` で包んで Gomocup エンジンにします。

## 準備

```bash
make bench                # bench/build/pbrain-legacy (授業版AIの Gomocup 版) をビルド
bench/setup_rapfi.sh      # 基準エンジン Rapfi を ../rapfi-src に取得し ../rapfi-bin にビルド
```

Rapfi の置き場所は環境変数 `RAPFI_DIR` で変えられます (既定は `<repo>/../rapfi-bin`)。

## ツール

| ファイル | 役割 |
|---|---|
| `match.py` | 2 エンジンの対局 (先後入替・並列)。勝敗、Elo 差と 95% 信頼区間、LOS、終局理由、思考時間を出す |
| `ladder.py` | 複数エンジンの総当たり/隣接対戦から Elo 表を推定 |
| `gen_openings.py` | Rapfi の評価で互角に近い開局を集める (`openings/*.txt`) |
| `show.py` | `games.jsonl` の棋譜を盤面表示 |
| `rules.py` | レフェリー: `freestyle` / `standard` / `renju` / `class` (授業審判そのもの) |
| `engine.py` | Gomocup エンジンのサブプロセスラッパー |
| `pbrain_adapter.c` | `calcPutPos` 形式の AI を Gomocup エンジンにするアダプタ |
| `bots/simple_bot.py` | 参照用の弱いボット (random / greedy) |
| `engines.json` | エンジン名とコマンドの登録表 |

## 使い方

```bash
# 現行AI vs 1手読みボット、連珠ルール、1手1秒、互角開局 20 個 x 先後 = 40 局、4 並列
python3 bench/match.py legacy greedy --rule renju --tc 1000 --openings bench/openings/renju_balanced.txt -j 4

# 授業審判 (check=7) で対局。黒の「授業審判だけが反則と判定する手」も負けになる
python3 bench/match.py legacy greedy --rule class --check 7

# ラダー (Elo 表)
python3 bench/ladder.py random legacy greedy rapfi-d2 rapfi-n1k rapfi-n10k --pairs adjacent --rule renju \
    --openings bench/openings/renju_balanced.txt

# 棋譜を見る
python3 bench/show.py bench/results/<dir>/games.jsonl 3 5
```

`--openings` は開局ファイルか数字 (その数のランダム開局) を受け付けます。
結果は `bench/results/<日時>_<A>_vs_<B>/` (git 管理外) に出ます。残したい結果は
`docs/benchmarks/` に要約を書いてください。

## ルール

| `--rule` | 内容 | Gomocup `INFO rule` |
|---|---|---|
| `freestyle` | 五連以上で勝ち | 0 |
| `standard` | ちょうど五連で勝ち (長連は勝ちでない) | 1 |
| `renju` | 黒の長連・四四・三三は禁じ手で負け (三三は「禁じ手でない手で達四にできる三」を再帰判定) | 4 |
| `class` | 授業の審判 `.libgomoku.c` の `gameEndProcess` を共有ライブラリにして直接呼ぶ。`--check` で禁じ手の種類 | 4 を送る |

`class` の審判は本物の連珠と判定が違います (例: `XX.XXXX` のように五にならない並びを四四と判定する)。
授業の大会向けの検証は必ず `class` で行ってください。

## 新しいエンジンを足すには

- Gomocup プロトコルを話す実行ファイルを作り、`engines.json` に `{"cmd": ..., "cwd": ..., "info": {...}}` を追加する
- `calcPutPos` 形式なら `pbrain_adapter.c` とリンクすればよい (Makefile の `bench` ターゲット参照)
- `info` は開始時に `INFO key value` として送られる (Rapfi なら `max_node`, `max_depth` で強さを絞れる)
- 手を返した直後に次のコマンドを受けると取りこぼすエンジン (Rapfi) は `pre_delay` (秒) を付ける

## 注意

- 時間切れは `tc × --time-factor + --time-margin` 秒を超えたら負け
- `pbrain-legacy` は対局ごとに一時ディレクトリで動くので、学習による
  `opening_book.csv` / `learning_data.dat` の書き換えはリポジトリに影響しない (`LEARNING_MODE=PRODUCTION`)
