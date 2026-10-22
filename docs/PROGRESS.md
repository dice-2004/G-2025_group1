# 進捗ログ

新しい順に書く。各エントリは「やったこと / 結果 (数字) / 次にやること」。
計画は [ROADMAP.md](./ROADMAP.md)、測定の詳細は [benchmarks/](./benchmarks/)。

## 現在のステータス

| 項目 | 状態 |
|---|---|
| フェーズ | Phase 0 (計測基盤) 完了 → Phase 1 (新エンジンの土台) 着手前 |
| 最強の自作エンジン | legacy (授業版 v3.4) |
| legacy の強さ | 連珠・1秒/手で Elo +883 (random=0)。1手読み貪欲ボットに 29.5/80、Rapfi 深さ2制限に 0/80 |
| 次の目標 | 新エンジンで greedy・legacy に大差勝ち、VCF テスト集を解く |

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
