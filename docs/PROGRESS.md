# 進捗ログ

新しい順に書く。各エントリは「やったこと / 結果 (数字) / 次にやること」。
計画は [ROADMAP.md](./ROADMAP.md)、測定の詳細は [benchmarks/](./benchmarks/)。

## 現在のステータス

| 項目 | 状態 |
|---|---|
| フェーズ | Phase 2 (本格探索) 項目 1 完了、評価の調整中 |
| 最強の自作エンジン | G2 v0.3 (engine/)。連珠・1秒/手で rapfi-n10k に 62勝17敗、rapfi-n100k に 38勝42敗 (Elo 約 +2680) |
| サイト AI との実戦 | T1 サイト (trygomoku/gomokuonline/playgomokuonline) は全勝。playgonu Pro は白番で負け、Rapfi NNUE 5 秒 (Champion/gomocalc) は 3-70 (gomocalc の持ち時間 100〜5000ms の全設定で敗退)。[結果](./benchmarks/web/RESULTS.md) |
| legacy の強さ | 連珠・1秒/手で Elo +883 (random=0)。1手読み貪欲ボットに 29.5/80、Rapfi 深さ2制限に 0/80 |
| 開局ブック | 連珠 2114・自由 1687 局面 (黒 7 手目・白 12 手目まで、Rapfi 古典評価で事前計算)。gomocalc 500ms 黒番 連珠 15-5 (なし 3-10)・自由 8-2 (なし 4-6)、白番は 0-30。[詳細](./benchmarks/2026-10-27_book.md) |
| 次の目標 | rapfi-n100k に勝ち越す (評価の再調整・探索の枝刈り・VCT の使い方) |

## 2026-10-27 開局ブック

**やったこと** (詳細は [benchmarks/2026-10-27_book.md](./benchmarks/2026-10-27_book.md)、実装は [engine/README.md](../engine/README.md))
- `engine/src/book.c`: 序盤 (1〜8 手目) の局面 → 最善手の表を埋め込み、実行時は表引きだけ (1 回 20µs 未満)。15 路、
  連珠と自由 (standard は自由を流用) の 2 表、8 対称・手順違いを正規化。`think_move` の先頭で引き、`INFO book 0` で無効。
  空点でない・黒の禁じ手になる手、五・達四の点がある局面では使わない
- `engine/book/gen_book.py`: Rapfi (古典評価、NNUE なし) で 1 局面 10〜30 秒の解析。G2 の手番は最善手、相手の手番は
  multi-PV 上位 3〜8 手とサイト AI の実戦の手をたどる (先後両方)。解析は `analysis_<rule>.jsonl` に貯めて再開・追加できる
- 照合 `check_book.py` (make test)、棋譜でのブックの当たりを数える `book_hits.py`

**結果**
- 連珠 515 局面、自由 444 局面。生成は 4 並列で計 45 分
- rapfi-n100k (古典) とブックあり/なし: 連珠 29-47 vs 33-43、自由 29-42 vs 28-43 (誤差内、差なし)
- サイト AI: gomocalc (NNUE) 500ms 連珠でブックあり 2-0-4 (黒 2 勝)、同じ版のブックなし 0-0-6。playgonu Pro 白番は
  連珠・自由とも 4-0 (ブックなしの v0.3 も 2-0 なので主に探索の改善)
- サイト実戦で G2 の 1〜4 手目は全部ブック、8 手目でも 29% が当たる

**追記 (同日・白番を 12 手目まで、標本追加)** 白の木だけ 12 手目まで生成 → 連珠 2114・自由 1687 局面。
gomocalc (NNUE) 500ms: 連珠 ブックあり **15-0-25** (黒 15-5) / なし 3-0-23 (黒 3-10)、自由 ブックあり **8-0-12** (黒 8-2) / なし 4-0-16 (黒 4-6)。
**黒番は gomocalc 500ms に勝ち越し**。白番はブックの有無にかかわらず 0-30 (開局の制約なしでは黒の必勝で、NNUE は白のどの応手にも勝ち筋を見つける)。
相手の活三への受けにもブックを使う `INFO book 2` は 6-14 で不採用 ([詳細](./benchmarks/2026-10-27_book.md))

**次にやること**
- 白番: NNUE 相手に粘れる 2・4 手目を実戦の勝率と手数で選ぶ、受けの探索の強化 (探索スレッド)
- gomocalc の持ち時間を上げて黒番の勝率を測る。サイト実戦の外れた局面を `gen_book.py --games` で足す

## 2026-10-22 サイト・アプリ AI との実戦ハーネス

**やったこと** (詳細は [bench/web/README.md](../bench/web/README.md)、結果は [benchmarks/web/RESULTS.md](./benchmarks/web/RESULTS.md))
- クラウド環境のネットワーク許可で対象サイト (playgonu / gomocalc / trygomoku / gomokuonline / playgomokuonline) に到達できることを確認
- `bench/web/play.py` + `sites/*.py`: Playwright (同梱 Chromium) で G2 をサイトの AI と自動対局。サイトごとのアダプタ (`new_game/place/wait_move/result`)
  で、盤は SVG / canvas / ページ内 JS の状態 / Vuex ストアから読む。サイトの勝敗は自前の審判 (`bench/rules.py`) とも照合する。
  棋譜は `docs/benchmarks/web/games/*.jsonl` (負け局を含む全局、`bench/show.py` と `engine/tests/analyze.py` で使える)
- gomocalc (Rapfi NNUE の Web 版) は「AI が黒/白」「1 手の持ち時間」を設定できる。T3 相手を持ち時間つきで指定できるので、強さの物差しに使える

**結果** (G2 v0.2、1 手 3〜5 秒)
| 対象 | 成績 (勝-分-敗) |
|---|---|
| trygomoku Master / gomokuonline Expert / playgomokuonline Lv10 | 全勝 (20-0-0) |
| playgonu Pro | 自由 6-0-2、連珠 2-0-4 (黒番 4-0、白番 4-6) |
| playgonu Champion (Rapfi NNUE 5 秒) | 0-0-7 (黒のみ) |
| gomocalc (Rapfi NNUE 5 秒) | 1-0-9 (自由・黒番の 1 勝のみ) |

- **G2 の白番が弱い**: 黒 7,7 への 2 手目が 5,5 (遠い斜め) で評価が -700〜-1700、17〜31 手で負ける局が多い
- G2 は `tc=5000` でも平均 2.7〜3.0 秒しか使わない。Champion は平均 3.3 秒 (最大 5 秒)
- アプリ (T2) はクラウドからは自動化不可 (KVM 無し、ストア入手不可)。代替案は [TARGETS.md](./TARGETS.md)

**追記 (2026-10-27)** gomocalc 持ち時間ラダー: 100/250/500/1500/5000 ms の全てで G2 は合計 3 勝 63 敗 (自由 3-0-29、連珠 0-0-34)。
最弱設定 (100 ms) にも届かないので、次は gomocalc の最大ノード数・最大深さを絞った弱い設定を物差しにする ([RESULTS](./benchmarks/web/RESULTS.md))。

**次にやること**
- 白番の序盤 (開局 DB / 近接応手の優先)、持ち時間の使い切り、連珠の受けを探索スレッドで改善
- gomocalc の `--level` (持ち時間) を変えて「G2 が勝てる最大の時間」を測り、進捗の物差しにする
- 改善のたびに `bench/web/play.py` で Pro 白番・Champion・gomocalc を再測定

## 2026-10-27 Phase 2-2: 評価の脅威加点・活三の延長・時間管理 (G2 v0.3)

**やったこと** (測定は [benchmarks/2026-10-22_search.md](./benchmarks/2026-10-22_search.md) の v0.3 節)
- 評価に「脅威の加点」を追加: 手番側・相手側それぞれ、次の 1 手で四三・三三・活三・四+・四 2 つを作れるか (空点の形の数から O(1))。
  重みは Rapfi (古典評価) の評価を正解ラベルにした調整 (`tune_eval.py --rapfi`) → 自己対戦 +117
- 活三を作る手の延長 (相手は受けるしかないので深さを減らさない) → 自己対戦 +132
- 時間管理: 次の反復を始めない目安を上限の 60% に (+44)。1 秒制限で最初の手が 1.1〜1.2 秒になる時間超過を修正
  (INFO 処理の時間を差し引き、置換表を毎局消さない、上限 64MB)
- 不採用: Texel の 2 回目 (-338)、相手 VCT への受けで候補を絞る (-48)、最善手が変わったときの時間延長 (-44)

**結果** (連珠、1 手 1 秒、80 局): rapfi-n10k に 62勝1分17敗 (+221)、rapfi-n100k に 38勝42敗 (-17、ほぼ互角)

**次にやること**
- 開局ブック (別スレッド) の組み込みと効果測定。白番の序盤
- rapfi-n100k に勝ち越す: 脅威の加点の種類を増やす、Rapfi ラベルを増やして再調整、VCT を長い持ち時間で再評価

## 2026-10-22 Phase 2-1: 本探索と評価関数 (G2 v0.2)

**やったこと** (詳細は [engine/README.md](../engine/README.md)、測定は [benchmarks/2026-10-22_search.md](./benchmarks/2026-10-22_search.md))
- `engine/src/search.c`: 反復深化 + PVS + aspiration window、Zobrist 置換表 (2 エントリのバケット、深さ・世代で置換)、
  強制手 (相手の五は止める手だけ・深さを減らさない、相手の活三には防ぎ手と自分の四だけ)、
  手の並べ替え (置換表の手 → パターン点 + killer + history)、LMR、浅い所の手数打ち切り
- ルートで詰み探索 (vcf.c) を使う: 自分の VCF、相手の VCF への受けに候補を絞る、(任意で) 自分の VCT
- `engine/src/eval.c`: 4 方向パターンの組 → 点数の表。盤面は空点の点数の和を差分更新。重みは Texel 法で調整
  (`engine/tests/tune_eval.py`、閾値・重みの軽い調整のみ)
- `engine/src/think.c`: 持ち時間 (timeout_turn / time_left) から 1 手の上限と「次の反復を始めない」目安を決める
- 高速化: 差分更新を変わった方向だけに、手生成を石の範囲だけに、手の並べ替えを遅延選択に → 探索全体で約 2 倍
- 道具: `INFO p_<名前> <値>` で探索・評価の値を変更、`bench/match.py` のエンジン名に `+key=value` / `@別名`、
  `engine/tests/analyze.py` (棋譜の局面を考えさせてログを見る)、`g2-test speed` (固定深さの速度)、`g2-test features`

**結果** (連珠、1 手 1 秒、80 局)
| 対戦 | G2 の成績 | Elo 差 |
|---|---|---|
| G2 v0.1 vs rapfi-d2 | 3勝1分76敗 | -536 |
| G2 v0.2 vs rapfi-n1k | 76勝4敗 | +512 |
| G2 v0.2 vs rapfi-n10k | 49勝31敗 | +80 |

- 速度: 固定深さ 8 の 16 局面で 4.15 秒 → 2.14 秒 (nps 約 42 万 → 83 万)、着手+戻し 158 万回/秒
- 自己対戦の A/B: Texel 調整の評価 +191、詰み探索の統合 +80、静止探索は -117〜-158 で不採用、VCT は 0.3 秒/手では効果なし

**次にやること**
- 新しい版の棋譜で評価を再調整 (Texel)、組み合わせの種類を増やす (Pattern4 の細分化)
- 探索の枝刈り (点の低い手を深い所で読まない)、VCT を長い持ち時間で再評価、相手の VCT への備え
- rapfi-n100k との対戦

## 2026-10-22 Phase 1-2: VCF / VCT 詰み探索

**やったこと** (詳細は [engine/README.md](../engine/README.md)、測定は [benchmarks/2026-10-22_tss.md](./benchmarks/2026-10-22_tss.md))
- `engine/src/vcf.c/.h`: VCF (四の連続) と VCT (四・活三の連続) の詰み探索
  - 連珠の禁じ手を攻守両側で考慮 (黒は禁じ手で四・三を作れない / 白の四の止め点が黒の禁じ手なら白の勝ち)
  - VCT は活三の受け (三の線上の点と逆四) を全部試す健全な証明。受け側に VCF があれば先に打ち切る
  - 置換表 (100 万エントリ)、VCT は深さ 1 からの反復深化
  - API: `vcf_solve` / `vct_solve` (手番側)、`tss_solve` (パスした場合の相手の詰みも)、`vcf_defenses` (相手の VCF を崩す手)
- テスト: 詰み問題集 `engine/tests/puzzles/tactics.txt` 99 問 (`make -C engine test` で実行)、
  Python 全探索・別実装レフェリー・Rapfi の詰み評価による照合ツール

**結果**
- VCF: Python 全探索と 3734 件で完全一致、手順 1135 本すべて正しい。1 局面 0.07ms
- VCT: Rapfi が詰みと評価した 581 局面のうち 88% (VCF だけなら 27%) を証明。誤った勝ち 0
- VCT は裾が重い (平均 97ms、100 万ノードで打ち切り 17/1507 局面)

**追記 (同日・速度)** dice の要望で速度も重視。VCT を同じ 100 万ノード上限で 146 秒 → 46 秒 (1507 局面、3.2 倍速)、
再現率 88% → 90%。1 万ノード上限なら 1 局面 2.9ms で 65%、10 万ノードで 11ms・85%。
`vcf_defenses` の取りこぼし (達四で終わる VCF の反対側の止め点) を修正し、合法手総当たりで 803 局面 0 件を確認。
測定は `engine/tests/tss_bench.sh` で再現できる ([benchmarks/2026-10-22_tss.md](./benchmarks/2026-10-22_tss.md))

**追記 (同日・VCT の受け)** `vct_defenses` (相手の VCF/VCT を崩し得る手) を追加。10 万ノードで 1 局面 42ms、
返さなかった手は「打っても相手に VCT がある」と証明済み。総当たり照合で 222 局面中 1 局面だけ遠くの逆四準備の手が漏れる。
長手数 (M21+) の取りこぼしの大半は脅威でない手を含む詰み (開局直後の必勝形) で VCT の範囲外と判明。
攻めの候補を広げる・四を深さに数えない案はどちらも再現率が下がったので不採用 (詳細は benchmarks)

**次にやること**
- 探索スレッドの PVS に組み込み (根と静止探索での VCF、相手 VCF の受けの絞り込み) → 対局で効果測定
- VCT の高速化 (着手・戻しのパターン更新が時間の 7 割。探索スレッドの表引き化に乗る) と長い詰み (M21+) の再現率改善

## 2026-10-22 Phase 1-1: 新エンジン G2 の土台

**やったこと** (詳細は [engine/README.md](../engine/README.md))
- `engine/` に新エンジン G2 を作成 (C11、外部ライブラリなし、最大 20 路)
  - 線パターン表: 中心 ±5 マスの並び (20 ビット) → 五/達四/一直線四四/四/活三/止め三/… を色・ルール別に表引き。生成 0.12 秒
  - 盤面: 着手/戻しで周囲 40 マスのキーとパターンを差分更新、Zobrist ハッシュ
  - ルール: freestyle / standard / renju と授業審判の `check` (1..7) を `Rule` で統一的に扱う
  - 連珠の禁じ手判定: 長連・四四 (一直線含む)・三三 (達四にする手が禁じ手なら三ではない、を再帰)
  - Gomocup 入口 `pbrain-g2`、授業の `calcPutPos` 入口 (`make g2class` → `./gomoku_g2`)
  - 暫定の指し手選択: 五 → 五止め → 達四 → 相手の活三への対応 → パターン点の 1 手読み
- テスト: `make -C engine test` (差分更新の自己検証、Python レフェリーとの禁じ手照合)

**結果** (1 手 1 秒、互角開局 40 x 先後)
| 対戦 | ルール | G2 の成績 | Elo 差 |
|---|---|---|---|
| G2 vs greedy | renju | 56勝11分13敗 | +209 |
| G2 vs legacy | renju | 64勝15分1敗 | +370 |
| G2 vs legacy | class (check=7) | 66勝11分3敗 | +370。G2 黒の授業審判反則負け 2 |
| G2 vs greedy | freestyle | 74勝3分3敗 | +490 |
| G2 vs rapfi-d2 | renju | 3勝1分76敗 | -536 |

- 禁じ手判定の照合: ランダム 2000 局面・39 万点で不一致 0
- legacy の新たな不具合: 終盤 (盤がほぼ埋まった時) に既に石のある (7,7) を返して反則負けになる

**次にやること**
- VCF/VCT ソルバと戦術テスト集 (Rapfi の詰み局面から自動収集)
- PVS + 置換表 + 評価関数 + 時間管理 (探索なしの今は rapfi-d2 に全く勝てない)
- 差分更新の高速化 (今は 1 手で 40 マス x 2 色の表引きと合成をしている)

## 2026-10-22 Phase 0: 現状把握・計測基盤

**やったこと**
- リポジトリ読解。ルールは授業の枠組み (15路、黒のみ禁じ手、`check` 変数で三三/四四/長連を切替)。
  AI は `calcPutPos` の中でルールベースの勝ち手/防御/攻撃判定 → 定石 → 反復深化 αβ
- `bench/` に計測基盤を作成 (詳細は [bench/README.md](../bench/README.md))
  - `pbrain_adapter.c`: 授業版 AI を Gomocup プロトコルのエンジン化 (`make bench`)
  - `rules.py`: freestyle / standard / renju (三三は再帰判定) / class (授業審判 `.libgomoku.c` を ctypes で直接呼ぶ)
  - `match.py` (先後入替・並列・Elo/CI/LOS)、`ladder.py` (Elo 表)、`gen_openings.py` (Rapfi 評価で互角開局)、`show.py`
  - 基準エンジン Rapfi (GPL、Gomocup 2025 全部門優勝) を `bench/setup_rapfi.sh` で外部にビルド。
    内蔵の古典評価で動かし、`max_node` / `max_depth` で強さを段階化
  - 互角開局をルール別に 40 個ずつ (`bench/openings/`)
- 既存 AI のバグ修正: `evaluate_position` のスタック破壊 (`int line[9]` に 11 マス書き込み)。
  ASan で検出。授業の大会でも誤動作の原因になり得た
- 目標サイト・アプリ調査 → [TARGETS.md](./TARGETS.md)。最上位 (T3) は中身が Rapfi
- ロードマップ作成 → [ROADMAP.md](./ROADMAP.md)

**結果** ([benchmarks/2026-10-22_baseline.md](./benchmarks/2026-10-22_baseline.md))
- 連珠ラダー (random=0): legacy +883 / greedy +975 / rapfi-d2 +1853 / rapfi-n1k +2077 /
  rapfi-n10k +2428 / rapfi-n100k +2700 / rapfi(1秒) +2882
- legacy vs rapfi-d2 は 0勝80敗。freestyle では greedy とほぼ互角 (36勝12分32敗)
- 授業審判と本物の連珠の食い違い: 実戦局面 836 個・黒の候補手 36,726 個で比較すると、
  両方が反則 94、授業審判だけが反則 27、連珠だけが反則 5。授業審判は連珠では合法な手の一部も負けにする

**わかった問題**
- 自前の禁じ手判定が三三を見逃す (legacy 黒番の負けの一部は三三の禁じ手)
- 探索が遅く浅い (ノードごとに全盤コピー・全盤評価)。手の並べ替え・PVS・VCF/VCT なし
- ルールベースのショートカットが探索結果より優先されてしまう

**ブロッカー**
- (解決済み) GitHub への push が 403 だった → 2026-10-22 に GitHub 再接続で解消
- クラウド環境のネットワーク方針で目標サイト (playgonu.com, gomocalc.com など) に接続できない。
  ブラウザ自動対局には Project settings でドメイン許可が必要

**次にやること** (ROADMAP Phase 1)
1. 新エンジンの盤面・差分パターン・ルール判定 + Gomocup 入口
2. VCF ソルバと戦術テスト集
3. その上に PVS + 置換表 + 評価関数
