# 対戦目標: 世界の五目並べサイト・アプリの最高難易度

最終目標は「Web サイトやアプリにある五目並べ AI の最高難易度に勝つ」こと、
その先は機械学習系トップエンジン (Rapfi NNUE 等) と同等の強さ。ここでは目標を
強さ別の段 (Tier) に分け、ルールと対戦方法をまとめる。2026-10-22 時点の調査。

## 強さの段

| Tier | 代表 | 中身 (公開情報) | 位置づけ |
|---|---|---|---|
| T1 | trygomoku.com Master, gomokuonline.org Expert, playgomokuonline.com Lv10 | minimax + αβ、深さ 4〜6 程度、脅威検出 | 最初の目標。きちんとした VCF/VCT 探索があれば勝てる見込み |
| T2 | 市販アプリ最上位: 「みんなの五目並べ」(UNBALANCE) Lv15、Gomoku Online (Best Caro) "Genius"、Gomoku Master、King Caro Master | 非公開。「人類最強クラス」をうたうもの多数 | 中間目標。強い古典エンジン (Rapfi 古典評価の短時間) 程度と想定 |
| T3 | playgonu.com "Champion (RAPFI)"、gomocalc (Rapfi の Web 版) | Rapfi = Gomocup 2025 全部門優勝。WASM でブラウザ内、1手約5秒 | 最終目標。NNUE 評価を使う世界最強級 |

参考: Gomocup 2025 は Freestyle/Standard/Renju/Fastgame の全部門で RAPFI が 1 位、
2 位 AlphaGomoku、3 位 JAX (いずれも NN 系)。古典系では Yixin, Embryo, Wine, SlowRenju が強豪。

## サイト・アプリごとのルール

| 名前 | 種類 | 盤 | ルール | 最高難易度 | 備考 |
|---|---|---|---|---|---|
| [playgonu.com/gomoku](https://playgonu.com/gomoku) | Web | 15 | freestyle / renju (swap なし) | Champion = RAPFI (約5秒/手) | Easy/Normal/Hard/Master/Pro/Champion |
| gomocalc (dhbloo) | Web | 15 他 | freestyle / standard / renju | Rapfi NNUE | Rapfi 作者の公式 Web 版。思考時間を指定可 |
| [trygomoku.com](https://trygomoku.com/) | Web | 9/15/19 | freestyle / renju | Master (ID 6層 + 強制手検出) | |
| [gomokuonline.org](https://gomokuonline.org/) | Web | 15 | 長連の扱い不明 (要確認) | Expert | Easy/Medium/Hard/Expert |
| [playgomokuonline.com](https://playgomokuonline.com/) | Web | 9〜19 | ちょうど五 / renju 任意 | Lv10 (深さスライダー) | minimax + αβ |
| [みんなの五目並べ](https://apps.apple.com/us/app/-/id1081747498) (UNBALANCE) | iOS/Android | 15 | 五目 / 連珠 | Lv15「人類最強クラス」 | |
| [Gomoku Online - Best Caro](https://apps.apple.com/app/id1197307658) | iOS | ? | caro 系 (ブロックルールは対人のみ) | Genius | 長連でも勝ちになる報告あり |
| [Gomoku Master](https://apps.apple.com/us/app/id6468030141) (CotGame) | iOS | 可変 | 連珠 | 不明 | |
| [King Caro](https://apps.apple.com/us/app/-/id6755077188) | iOS | ? | 五目 | Master | |
| Yixin Board (Windows) | デスクトップ | 15/20 | freestyle / standard / renju | Yixin (古典の最強級) | Gomocup 2012-2018 freestyle 連覇 |

T1 の Web 版はルール設定に注意: 「長連は勝ちか」「黒の禁じ手があるか」で必要な AI が変わる。
エンジンは freestyle / standard / renju を切り替えられるように作る (ROADMAP の Phase 1)。

## 実戦結果 (G2 v0.2、2026-10-22)

詳細は [benchmarks/web/RESULTS.md](./benchmarks/web/RESULTS.md)、棋譜は `docs/benchmarks/web/games/`、ハーネスは [bench/web/](../bench/web/README.md)。
勝-分-敗は G2 から見た成績 (1 手 3〜5 秒)。

| 対象 | 最高難易度 | ルール | G2 の成績 | 判定 |
|---|---|---|---|---|
| trygomoku.com | Master | freestyle / renju | 4-0-0 / 4-0-0 (先後) | 勝ち越し (T1 突破) |
| gomokuonline.org | Expert | freestyle | 4-0-0 (黒のみ) | 勝ち越し (T1 突破) |
| playgomokuonline.com | Lv10 | freestyle / renju | 4-0-0 / 4-0-0 (先後) | 勝ち越し (T1 突破) |
| playgonu.com | Pro | freestyle / renju | 6-0-2 / 2-0-4 (先後) | **白番で負ける** (黒番 4-0、白番 4-6) |
| playgonu.com | Champion (RAPFI NNUE 5秒) | freestyle / renju | 0-0-4 / 0-0-3 (黒のみ) | 未達 (T3) |
| gomocalc.com | Rapfi NNUE 5秒 2 スレッド | freestyle / renju | 1-0-3 / 0-0-6 (先後) | 未達 (T3) |
| アプリ各種 (T2) | — | — | 未計測 | 下記「アプリ」参照 |

ポイント: (1) T1 は突破。(2) 次に倒すべきは playgonu Pro の白番と、T3 (Rapfi NNUE 5 秒)。
(3) どの相手に対しても **G2 の白番が弱い** (序盤 2 手目が 5,5 のような遠い応手)。
(4) G2 は 1 手 5 秒の設定でも平均 2.7〜3.0 秒しか使わない。Champion/gomocalc は 5 秒近く使う。
Champion (playgonu) と gomocalc は同じ Rapfi NNUE (Mix9svq) で、**ローカルの Rapfi 古典評価 (bench/setup_rapfi.sh) より強い**ので、
ローカルの `rapfi` 勝敗はここの目標の代わりにならない。進捗の物差しとしては gomocalc の `--level <ms>` (1 手の持ち時間)
を変えて勝てる最短時間を測るのが使える。

### サイトごとの実装メモ

| サイト | 規則・仕様 | 自動化の方法 |
|---|---|---|
| playgonu | 15 路、freestyle / renju (swap なし、黒の初手は中央固定、禁じ手は打てない)。Champion は人間=黒固定、ブラウザ内 WASM (Rapfi NNUE)、1 手 5 秒固定 | SVG から石を読み `.board-point` を click |
| trygomoku | 9/15/19、freestyle / renju。Master は反復深化 2→12 手読み (最大 12 秒) + VCF/脅威検出の JS AI、決定的に見える | ページ内グローバル `board`/`placeStone` |
| gomokuonline | 15 路、freestyle のみ (五連以上で勝ち、禁じ手なし)、AI は常に白。Expert は advanced-ai.js (深さ 10、約 3 秒)、決定的に見える | `window.app` |
| playgomokuonline | 9〜19 路、強さ 1..10、禁じ手 (Renju) はチェックボックス、AI は Web Worker | canvas 座標 click と MOVE HISTORY 文字列 |
| gomocalc | Rapfi NNUE の解析 Web 版。AI の手番設定 (黒/白/両方)、持ち時間・深さ・ノード・スレッド・ルール (freestyle/standard/renju/swap1) を指定可能。**人と対戦する用の最強設定が作れる** | Vuex ストアを直接操作 |

### アプリ (みんなの五目並べ、Gomoku Online (Best Caro)、Gomoku Master、King Caro など)

クラウド環境からは自動対戦できない: Android/iOS の実機・エミュレータが無い (`/dev/kvm` が無く x86 エミュレータは実用速度で動かない)、
ストアからアプリ本体を取得できない、Web 版が見つからない (調べた範囲)。代替方針:

1. **強さの代理指標で判断する**: 「みんなの五目並べ」は App Store の説明でレベル 15 に Remi Coulom 氏の連珠 AI を搭載とある。
   他のアプリの最高難易度も、公開情報では minimax 系か不明。Rapfi NNUE (gomocalc/Champion) に勝てる強さに達すれば
   これらのアプリの最高難易度より強いとみなす (T3 を突破してから確認)。
2. **ローカル PC がある場合の自動化案** (dice の手元でだけ可能。必須ではない): Android エミュレータ (KVM 有効) + adb で
   画面を撮ってスクリーンショットから盤を読み取り、タップで着手する。ハーネスの `Site` インターフェースで実装できる
   (`new_game/place/wait_move/result`)。
3. 最終確認だけ人手: T3 突破後にスマホで数局打つだけにする。

## 対戦のしかた

1. **ブラウザ自動操作 (実装済み・2026-10-22 時点で全サイトに到達可)**: `python3 bench/web/play.py <サイト> --level <難易度> --rule <規則> ...`
   ([bench/web/README.md](../bench/web/README.md))。Playwright + 同梱 Chromium でヘッドレス対局し、棋譜を
   `docs/benchmarks/web/games/` に追記する。サイトの AI はこちらの CPU で動くので並列は 2 までにする。
2. **ローカル代理**: Rapfi (古典評価) を `bench/setup_rapfi.sh` でビルドして `bench/match.py` で対局。ノード数を絞った
   `rapfi-n*` は T1〜T2 相当の相手。ただし NNUE ではないので T3 の代わりにはならない。
3. **手動 (アプリ)**: 上記「アプリ」参照。

## 未確認・要調査

- gomokuonline.org の長連・禁じ手の扱い → freestyle (五連以上で勝ち) で確認済み (ai/game.js の checkWin が count>=5)
- 同じ局面で毎回同じ手を打つか → trygomoku Master / gomokuonline Expert は決定的に見える (G2 が同じ手なら同じ棋譜になる。
  gomokuonline Expert は 4 局中 3 局が同じ 19 手)。Champion と gomocalc は G2 の手が時間で揺れるせいもあり別の局になる。
  厳密には同じ局面をサイトに複数回与えて確かめる必要がある
- 中国系アプリ (弈心 Yixin モバイル、各種「五子棋」アプリ) の最高難易度
- gomocup.org から強い古典エンジン (Yixin、Embryo 等) を落として T2 のローカル代理にできるか (サイトには到達可。Windows バイナリが多く、Linux で動くかは未確認)
