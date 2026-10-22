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

## 対戦のしかた

1. **ローカル代理 (今すぐできる)**: T3 の中身は Rapfi なので、ローカルにビルドした Rapfi
   (`bench/setup_rapfi.sh`) を相手に `bench/match.py` で自動対局する。
   ノード数や時間を絞れば T1〜T2 相当の相手も作れる (`engines.json` の `rapfi-n*`)。
   NNUE 重み (rapfi-networks) を入れれば T3 そのものに近くなる。
2. **ブラウザ自動操作 (要ネットワーク許可)**: Playwright + Chromium で Web サイトの盤面を読み、
   自エンジンの手をクリックする。今のクラウド環境はネットワーク方針で上記サイトに届かない
   (403)。使うには Project settings の Cloud environment で該当ドメインを許可する必要がある。
3. **手動 (アプリ)**: スマホアプリは自動化が難しいので、エンジンの手を人が打ち込む。
   最終確認用に限る。

## 未確認・要調査

- 各サイトの思考時間と、同じ局面で毎回同じ手を打つか (決定的か)
- gomokuonline.org の長連・禁じ手の扱い
- 中国系アプリ (弈心 Yixin モバイル、各種「五子棋」アプリ) の最高難易度
