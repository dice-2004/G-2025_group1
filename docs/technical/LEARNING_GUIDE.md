# 五目並べAI - 学習機能ガイド

## 概要

このAIは**対戦するほど強くなる学習機能**を搭載しています。対戦を重ねることで、以下のような学習が自動的に行われます：

1. **パターンスコアの自動調整** - 勝敗に応じて評価関数のパターンスコアを最適化
2. **序盤定石の学習** - 勝った試合の序盤手を自動的に定石として記憶
3. **学習データの永続化** - 学習内容をファイルに保存し、次回起動時に引き継ぐ

## 学習の仕組み

### 1. パターンスコア自動調整

AIは対戦結果を分析し、使用したパターンの評価値を動的に調整します：

- **勝った場合**: 使ったパターンのスコアを強化（増加）
- **負けた場合**: 使ったパターンのスコアを弱化（減少）

調整は以下の式で行われます：

```
新スコア = 現在のスコア + (調整値 × 学習率 × 勝敗係数)
```

- **学習率**: 対戦回数が増えるほど小さくなる（過学習防止）
- **調整値**: パターンごとに異なる（OPEN_FOUR は大きく、ONE は小さく）
- **勝敗係数**: 勝ち=+1.0、負け=-1.0

### 2. 対戦フェーズ別学習

AIは対戦を3つのフェーズに分けて分析します：

| フェーズ | 手数 | 学習するパターン |
|---------|------|----------------|
| 序盤 | 1-10手 | ONE, TWO |
| 中盤 | 11-20手 | OPEN_TWO, THREE, OPEN_THREE |
| 終盤 | 21手以降 | FOUR, OPEN_FOUR |

各フェーズで使われたパターンの成功/失敗を記録し、スコア調整に反映します。

### 3. 序盤定石の自動追加

AIが**勝った試合**の序盤5手を自動的に `opening_book.csv` に追加します：

```csv
move_number,x,y,comment
1,7,7,Learned from win (game 15)
2,8,7,Learned from win (game 15)
3,7,8,Learned from win (game 15)
```

次回以降の対戦では、この定石が参照されて同じ展開を試みます。

### 4. 学習データの保存

学習内容は `learning_data.dat` に自動保存されます：

- **対戦統計**: 総試合数、勝敗数、勝率
- **パターン学習データ**: 各パターンの調整値、成功/失敗回数

AIを再起動しても、過去の学習内容が引き継がれます。

## 学習の進行

### 初回起動時

```
[Learning] Learning system initialized.
[Learning] No previous learning data found. Starting fresh.
```

AIは初期状態から学習を開始します。

### 学習中（対戦終了時）

```
========================================
[Learning] Game finished! Result: WIN
[Learning] Total games: 12 (W:8 L:4 D:0)
[Learning] Win rate: 66.67%
[Learning] Applying pattern adjustments (factor=0.0144)...
[Learning] Pattern scores updated:
  FIVE: 100000, OPEN_FOUR: 50250, FOUR: 10150
  OPEN_THREE: 5180, THREE: 1090, OPEN_TWO: 520
  TWO: 108, ONE: 12
[Learning] Added winning opening moves to book.
[Learning] Learning data saved successfully.
========================================
```

3試合ごとにパターンスコアが更新されます。

### 次回起動時

```
[Learning] Loaded previous learning data:
  Total games: 12 (W:8 L:4 D:0)
  Win rate: 66.67%
[Learning] Applying pattern adjustments (factor=0.0144)...
[Learning] Pattern scores updated:
  FIVE: 100000, OPEN_FOUR: 50250, FOUR: 10150
  ...
```

過去の学習内容が自動的に復元されます。

## 学習パラメータ

### 調整可能なパラメータ（`logics/learning.c` 内）

```c
#define LEARNING_RATE 0.05           // 基本学習率
#define PATTERN_ADJUST_RATE 0.10     // パターン調整率
#define MIN_GAMES_FOR_LEARNING 5     // 学習開始に必要な試合数
```

| パラメータ | 説明 | デフォルト値 | 推奨範囲 |
|-----------|------|-------------|---------|
| `LEARNING_RATE` | 学習の速度（大きいほど速く変化） | 0.05 | 0.01 - 0.15 |
| `PATTERN_ADJUST_RATE` | パターンスコアの調整幅 | 0.10 | 0.05 - 0.20 |
| `MIN_GAMES_FOR_LEARNING` | 学習開始までの試合数 | 5 | 3 - 10 |

### パラメータ変更方法

1. `logics/learning.c` をテキストエディタで開く
2. 上記の `#define` 行を変更
3. `build.bat` を実行して再コンパイル

例：学習を速くする場合
```c
#define LEARNING_RATE 0.10           // 2倍の速度
#define PATTERN_ADJUST_RATE 0.15     // 1.5倍の調整幅
```

## 学習状況の確認

### 1. 学習統計の表示

対戦終了時に自動表示されます：

```
Total games: 25 (W:16 L:9 D:0)
Win rate: 64.00%
```

### 2. パターンスコアの変化

3試合ごとに表示されます：

```
Pattern scores updated:
  FIVE: 100000, OPEN_FOUR: 50450, FOUR: 10280
  OPEN_THREE: 5320, THREE: 1180, OPEN_TWO: 560
  TWO: 115, ONE: 13
```

初期値からの変化を確認できます。

### 3. 学習した定石の確認

`opening_book.csv` を開いて確認：

```bash
type opening_book.csv
```

末尾に `Learned from win (game N)` というコメントが付いた手が学習された定石です。

## 学習データのリセット

学習をやり直したい場合：

### 方法1: 学習データファイルの削除

```bash
del learning_data.dat
```

次回起動時に新規学習が開始されます。

### 方法2: 学習済み定石のリセット

```bash
del opening_book.csv
```

`patterns.csv` から初期定石が再読み込まれます（学習統計は保持）。

### 方法3: 完全リセット

```bash
del learning_data.dat
del opening_book.csv
```

すべての学習内容がクリアされます。

## 学習の効果

### 短期効果（5-10試合後）

- 初歩的なミスが減少
- よく使うパターンの評価が最適化
- 勝利パターンの再現率向上

### 中期効果（20-50試合後）

- 勝率が安定
- 序盤定石のバリエーション増加
- 対戦相手の特徴に適応

### 長期効果（100試合以上）

- 勝率が収束（理論上の最適値に近づく）
- 豊富な定石データベース
- 状況に応じた最適な評価

## 学習の限界

### 学習できること

✅ パターンの評価値調整
✅ 勝利した序盤の手順
✅ 対戦相手の特徴への適応

### 学習できないこと

❌ 全く新しい戦術の発明
❌ 禁じ手ルールの変更
❌ 探索深度の自動調整

学習機能は既存の評価関数を最適化するものであり、AIの基本的なアルゴリズムは変更されません。

## トラブルシューティング

### Q: 学習が反映されない

**A**: 以下を確認してください：
1. `MIN_GAMES_FOR_LEARNING` 以上の試合数があるか
2. `learning_data.dat` が正しく保存されているか
3. 3試合ごとに更新されるため、1-2試合では変化が見えない

### Q: スコアが異常に大きく/小さくなった

**A**: 学習率が大きすぎる可能性があります：
```c
#define LEARNING_RATE 0.02  // より小さく
```

また、以下のコマンドで学習データをリセット：
```bash
del learning_data.dat
```

### Q: 勝率が上がらない

**A**: 以下の要因が考えられます：
- 対戦相手が強すぎる（人間の上級者など）
- 探索深度が不足（`config.txt` で `SEARCH_DEPTH` を増やす）
- 学習データが不足（50試合以上推奨）

## 高度な使い方

### カスタム学習戦略

`logics/learning.c` の `analyze_game_patterns()` 関数を編集することで、学習戦略をカスタマイズできます：

```c
// 例：終盤重視の学習
if (g_current_game.move_count > 30) {
    g_pattern_learning.score_adjustment[2] += factor * 5.0;  // FOURを大きく調整
    g_pattern_learning.score_adjustment[1] += factor * 8.0;  // OPEN_FOURをさらに大きく
}
```

### 学習統計の詳細分析

学習データをプログラムで読み取って分析する例：

```c
LearningStats stats = get_learning_stats();
printf("Win rate: %.2f%%\n", stats.win_rate * 100.0);
printf("Total games: %d\n", stats.total_games);
```

## まとめ

この学習機能により、AIは対戦を重ねるごとに：

1. **評価関数が最適化**され、より正確な局面判断が可能に
2. **序盤定石が充実**し、スムーズな立ち上がりが可能に
3. **対戦相手に適応**し、特定の相手に対する勝率が向上

初期状態では弱いAIでも、50-100試合後には顕著な強化が期待できます。ぜひ何度も対戦して、AIを育ててください！

---

**関連ファイル**:
- `logics/learning.h` - 学習機能のインターフェース
- `logics/learning.c` - 学習アルゴリズムの実装
- `learning_data.dat` - 学習データの保存ファイル（バイナリ）
- `opening_book.csv` - 序盤定石データベース（テキスト）

**参考**: `QUICKSTART.md` も併せてご覧ください。
