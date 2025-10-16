# 機能詳細

このドキュメントでは、五目並べAI v3.4のすべての機能について詳しく説明します。

---

## 📋 目次

1. [探索・評価機能](#探索評価機能)
2. [学習機能](#学習機能)
3. [デバッグ機能](#デバッグ機能)
4. [時間管理機能](#時間管理機能)
5. [設定管理機能](#設定管理機能)

---

## 探索・評価機能

### ビットボード

**概要:**
- 盤面を64bitデータで表現
- ビット演算による高速処理

**利点:**
- メモリ効率が良い
- 高速な盤面操作

**実装ファイル:**
- `gomoku.c/h`

### Alpha-Beta探索

**概要:**
- Minimax探索の最適化版
- 不要な探索枝を枝刈り

**利点:**
- 探索ノード数を大幅削減
- 理論的には√n に削減可能

**実装ファイル:**
- `logics/main_engine.c/h`

**設定:**
```txt
SEARCH_DEPTH=4    # 探索深度
```

詳しくは → [AI実装の技術詳細](../technical/AI_IMPLEMENTATION.md)

### 置換表（Zobrist Hashing）

**概要:**
- 探索済み局面の評価値をキャッシュ
- Zobrist Hashingで盤面をハッシュ化

**利点:**
- 同一局面の再計算を回避
- 2-5倍の高速化

**実装ファイル:**
- `logics/transposition.c/h`

**統計情報:**
```
[Search] Trans hits: 567 (45.9%)
```

詳しくは → [AI実装の技術詳細](../technical/AI_IMPLEMENTATION.md)

### 序盤定石

**概要:**
- データベース化された序盤手順
- 効率的な序盤展開

**データファイル:**
- `opening_book.csv`

**実装ファイル:**
- `logics/opening_book.c/h`

**使い方:**
- 自動的に適用される
- カスタマイズ可能

詳しくは → [使い方ガイド](./USAGE_GUIDE.md#序盤定石の追加)

### 20種類の評価パターン

**概要:**
- v3.4で8パターン → 20パターンに拡張
- 複合パターン、飛びパターン、位置評価に対応

#### 基本パターン（8種類）

| パターン | スコア | 説明 |
|---------|--------|------|
| FIVE | 100000 | 五（勝ち） |
| OPEN_FOUR | 50000 | 開いた四（次に勝ち） |
| FOUR | 10000 | 四 |
| OPEN_THREE | 5000 | 開いた三 |
| THREE | 1000 | 三 |
| OPEN_TWO | 500 | 開いた二 |
| TWO | 100 | 二 |
| ONE | 10 | 一 |

#### 複合パターン（3種類）

| パターン | スコア | 説明 |
|---------|--------|------|
| DOUBLE_FOUR | 45000 | 四四（確実な勝ち） |
| FOUR_THREE | 15000 | 四三（強力な攻め） |
| DOUBLE_THREE | 3000 | 三々（攻めの形） |

#### 飛びパターン（3種類）

| パターン | スコア | 説明 |
|---------|--------|------|
| JUMP_FOUR | 8000 | 飛び四（●●_●●） |
| JUMP_THREE | 800 | 飛び三（●●_●） |
| JUMP_TWO | 80 | 飛び二（●_●） |

#### 位置評価（6種類）

| パターン | スコア | 説明 |
|---------|--------|------|
| CENTER_BONUS | 100 | 中央付近（有利な位置） |
| CONNECTION_BONUS | 50 | 連結評価（石の連なり） |
| NEAR_ENEMY_BONUS | 30 | 敵石近接（攻防の要点） |
| CORNER_PENALTY | -50 | 隅のペナルティ |
| EDGE_PENALTY | -30 | 端のペナルティ |
| ISOLATION_PENALTY | -20 | 孤立ペナルティ |

**データファイル:**
- `patterns_extended.csv`

**実装ファイル:**
- `logics/evaluation.c/h`

詳しくは → [評価パターンガイド](../technical/EVALUATION_PATTERNS.md)

### 11マス抽出 + 6マス窓

**概要:**
- v3.4で実装された最適化手法
- 中心石から前後5マスずつ抽出（計11マス）
- 6マス窓をスライドさせて評価

**利点:**
- 「開いた四」を正確に検出可能
- 両側の状況を確認可能
- 多角的な評価

**実装詳細:**

1. **11マス抽出**
   ```c
   // 中心石から前後5マスずつ
   int nx = x - dx * 5;  // 5マス戻る
   // 11マス抽出
   ```

2. **6マス窓スライディング**
   ```c
   // 6マス窓を6回スライド
   for (int i = 0; i <= 5; i++) {
       // window = line[i..i+5]
       // パターンマッチング
   }
   ```

**検証レポート:**
- [窓サイズ詳細分析](../verification/WINDOW_SIZE_ANALYSIS.md)
- [11マス抽出+6マス窓実装報告](../verification/IMPLEMENTATION_REPORT_11x6.md)

### TSS（詰み探索）

**概要:**
- 必勝手順の探索
- 確実な勝ちを見つける

**実装ファイル:**
- `logics/TSS.c/h`

**動作:**
- 自動的に適用される
- 詰みが見つかれば即座に返す

### 禁じ手判定

**概要:**
- 五目並べルールの完全実装
- 黒の禁じ手を判定

**禁じ手の種類:**
- 三々（Double Three）
- 四々（Double Four）
- 長連（Overline）

**実装ファイル:**
- `gomoku.c/h`

---

## 学習機能

### 勝敗分析

**概要:**
- 各試合の結果を記録
- 勝率を計算

**出力例:**
```
[Learning] Total games: 12 (W:8 L:4 D:0)
[Learning] Win rate: 66.67%
```

### パターン評価

**概要:**
- 使用したパターンを分析
- 効果的なパターンを特定

**動作:**
- 各試合でどのパターンが使われたか記録
- 勝った試合のパターンを重視

### スコア調整

**概要:**
- 勝率に応じて評価値を最適化
- 段階的な強化学習

**調整方法:**
- 勝った試合: スコアを +5% 増加
- 負けた試合: スコアを -2% 減少

**出力例:**
```
[Learning] Pattern scores updated:
  OPEN_FOUR: 50250, FOUR: 10150, OPEN_THREE: 5180
```

### 定石学習

**概要:**
- 勝った手順を記憶
- `opening_book.csv`に保存

**動作:**
- 勝った試合の最初の5手を記録
- スコアを付けて保存

### データ保存

**概要:**
- 学習データをファイルに保存
- 次回起動時に引き継ぎ

**ファイル:**
- `learning_data.txt`

**フォーマット:**
```
total_games=12
wins=8
losses=4
draws=0
OPEN_FOUR=50250
FOUR=10150
...
```

### 学習モード切替

**PRACTICE モード:**
- 対戦データから学習
- スコアを調整
- 探索的な動作

**PRODUCTION モード:**
- 学習済みデータを使用
- スコアを変更しない
- 安定した動作

**設定:**
```txt
LEARNING_MODE=PRACTICE    # または PRODUCTION
```

詳しくは → [学習モード切替ガイド](./LEARNING_MODE_GUIDE.md)

---

## デバッグ機能

### デバッグレベル

**レベル0（OFF）:**
- デバッグ出力なし
- 本番モード

**レベル1（基本）:**
- 探索深度
- 探索ノード数
- 置換表ヒット率
- 最善手

出力例:
```
[ID] Depth 2 completed in 0.15s (score: 200)
[Search] Nodes: 1234, Trans hits: 567 (45.9%), Best move: H8
```

**レベル2（詳細）:**
- レベル1の内容
- 評価関数の詳細
- パターンマッチング結果

出力例:
```
[Eval] Position H8: score=1250
  OPEN_THREE: 1, THREE: 2, TWO: 3
  CENTER_BONUS: 100
```

**レベル3（超詳細）:**
- レベル2の内容
- すべての探索ノード情報
- Alpha-Beta枝刈りの詳細

出力例:
```
[Search] Node: depth=2, alpha=-500, beta=500
[Search] Move H8: score=1250
[Search] Alpha updated: -500 -> 1250
[Search] Beta cutoff at H9
```

**設定:**
```txt
DEBUG_LEVEL=1    # 0-3
```

詳しくは → [デバッグ&IDガイド](./DEBUG_AND_ID_GUIDE.md)

---

## 時間管理機能

### Iterative Deepening

**概要:**
- 深度1から順に探索
- 時間制限に柔軟に対応

**利点:**
- 時間切れでも最善手が得られる
- 浅い深度の結果を深い深度で活用

**動作:**
```
Depth 1 → 完了（0.05秒）
Depth 2 → 完了（0.15秒）
Depth 3 → 完了（0.45秒）
Depth 4 → 時間切れ → Depth 3の結果を返す
```

詳しくは → [デバッグ&IDガイド](./DEBUG_AND_ID_GUIDE.md)

### ID時間閾値

**概要:**
- 次の深度に進むかの判断基準
- 残り時間の割合で指定

**設定:**
```txt
ID_TIME_THRESHOLD=0.8    # 0.0-1.0
```

**動作:**
```
残り時間 > 予想時間 * 閾値 → 次の深度へ
残り時間 ≤ 予想時間 * 閾値 → 現在の深度で終了
```

**推奨値:**
- **0.7**: 保守的（早めに探索を打ち切る）
- **0.8**: 推奨（バランス良く探索）
- **0.9**: 積極的（ギリギリまで探索）

詳しくは → [ID時間管理ガイド](./ID_TIME_MANAGEMENT.md)

---

## 設定管理機能

### CSV設定ファイル

**概要:**
- 外部ファイルで設定を管理
- プログラムの再コンパイル不要

**設定ファイル:**

1. **config.txt** - AI設定
   ```txt
   SEARCH_DEPTH=4
   DEBUG_LEVEL=1
   LEARNING_MODE=PRACTICE
   ID_TIME_THRESHOLD=0.8
   ```

2. **patterns.csv** - 基本パターンスコア
   ```csv
   PATTERN_NAME,SCORE,DESCRIPTION
   FIVE,100000,五
   OPEN_FOUR,50000,開いた四
   ...
   ```

3. **patterns_extended.csv** - 拡張パターンスコア
   ```csv
   PATTERN_NAME,SCORE,DESCRIPTION
   DOUBLE_FOUR,45000,四四
   FOUR_THREE,15000,四三
   ...
   ```

4. **opening_book.csv** - 序盤定石
   ```csv
   MOVE_SEQUENCE,SCORE,COMMENT
   H8,100,中央
   H7-I8,90,斜め配置
   ...
   ```

**実装ファイル:**
- `config.c/h`

詳しくは → [使い方ガイド](./USAGE_GUIDE.md#設定方法)

---

## 統計情報

### 探索統計

**出力項目:**
- 探索ノード数
- 置換表ヒット数・ヒット率
- 探索時間
- 最善手とスコア

**出力例:**
```
[Search] Nodes: 1234, Trans hits: 567 (45.9%)
[Search] Time: 0.15s, Best move: H8 (score: 200)
```

### 学習統計

**出力項目:**
- 総試合数
- 勝敗数
- 勝率
- パターンスコア更新

**出力例:**
```
[Learning] Total games: 12 (W:8 L:4 D:0)
[Learning] Win rate: 66.67%
[Learning] Pattern scores updated:
  OPEN_FOUR: 50250, FOUR: 10150, OPEN_THREE: 5180
```

---

## 関連ドキュメント

- [プロジェクト概要](./PROJECT_OVERVIEW.md)
- [使い方ガイド](./USAGE_GUIDE.md)
- [完全技術資料](../technical/TECHNICAL_DOCUMENT.md)
- [AI実装の技術詳細](../technical/AI_IMPLEMENTATION.md)
- [学習機能詳細](../technical/LEARNING_GUIDE.md)
- [評価パターンガイド](../technical/EVALUATION_PATTERNS.md)
