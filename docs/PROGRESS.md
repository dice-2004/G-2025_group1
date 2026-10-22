# 進捗ログ

新しい順に書く。各エントリは「やったこと / 結果 (数字) / 次にやること」。
計画は [ROADMAP.md](./ROADMAP.md)、測定の詳細は [benchmarks/](./benchmarks/)。

## 現在のステータス

| 項目 | 状態 |
|---|---|
| フェーズ | Phase 1 (新エンジンの土台) 項目 1 完了 → VCF ソルバ / 本格探索へ |
| 最強の自作エンジン | G2 v0.1 (engine/)。連珠で legacy に 64勝15分1敗 |
| legacy の強さ | 連珠・1秒/手で Elo +883 (random=0)。1手読み貪欲ボットに 29.5/80、Rapfi 深さ2制限に 0/80 |
| 次の目標 | VCF/VCT ソルバと PVS 探索で rapfi-d2 → rapfi-n1k に勝ち越す |

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
- GitHub への push が 403 (アカウント再接続待ち)。コミットはローカルに保持
- クラウド環境のネットワーク方針で目標サイト (playgonu.com, gomocalc.com など) に接続できない。
  ブラウザ自動対局には Project settings でドメイン許可が必要

**次にやること** (ROADMAP Phase 1)
1. 新エンジンの盤面・差分パターン・ルール判定 + Gomocup 入口
2. VCF ソルバと戦術テスト集
3. その上に PVS + 置換表 + 評価関数
