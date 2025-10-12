# 学習モード切り替え機能ガイド

## 概要

このGomoku AIは、**練習モード（PRACTICE）** と **本番モード（PRODUCTION）** の2つの学習モードをサポートしています。

## 学習モードの違い

### 📚 練習モード（PRACTICE）- デフォルト

```
目的: AIの学習・トレーニング
動作:
  ✅ 学習データを参照する
  ✅ 対戦結果から学習する
  ✅ パターンスコアを自動調整する
  ✅ 序盤定石を自動学習する
  ✅ 学習データを保存する

使用場面:
  - 日常の練習対戦
  - AIのトレーニング期間
  - 新しい戦術の学習
  - 性能向上のための対局
```

### 🏆 本番モード（PRODUCTION）

```
目的: 安定した性能での本番対戦
動作:
  ✅ 学習データを参照する
  ❌ 対戦結果から学習しない
  ❌ パターンスコアを変更しない
  ❌ 序盤定石を更新しない
  ❌ 学習データを保存しない

使用場面:
  - 大会・トーナメント
  - 正式な対戦
  - 性能評価テスト
  - 学習済みAIの実力発揮
```

## 設定方法

### 方法1: config.txtファイルを編集

`config.txt` ファイルを開いて、`LEARNING_MODE` の値を変更します：

```txt
# 学習モード（PRACTICE=練習モード, PRODUCTION=本番モード）
# PRACTICE: 学習しながら学習データを参照（練習・トレーニング用）
# PRODUCTION: 学習せず学習データのみ参照（本番・大会用）
LEARNING_MODE=PRACTICE
```

**練習モードに設定する場合:**
```txt
LEARNING_MODE=PRACTICE
```

**本番モードに設定する場合:**
```txt
LEARNING_MODE=PRODUCTION
```

### 方法2: コードで直接変更（開発者向け）

`config.c` または `gomoku_ai.c` で直接設定を変更できます：

```c
// config.c のデフォルト設定
g_config.learning_mode = LEARNING_MODE_PRACTICE;  // 練習モード
// または
g_config.learning_mode = LEARNING_MODE_PRODUCTION;  // 本番モード
```

## 動作確認方法

プログラムを起動すると、現在の学習モードが表示されます：

### 練習モードの場合

```
[Config] Loaded from config.txt
  SEARCH_DEPTH = 4
  TT_SIZE = 262144
  DEBUG_MODE = 0
  TIMEOUT_MS = 10000
  LEARNING_MODE = PRACTICE (練習)

[Learning] Learning mode set to: PRACTICE (練習)
[Learning] Practice mode: Learning enabled, data will be updated.
```

### 本番モードの場合

```
[Config] Loaded from config.txt
  SEARCH_DEPTH = 4
  TT_SIZE = 262144
  DEBUG_MODE = 0
  TIMEOUT_MS = 10000
  LEARNING_MODE = PRODUCTION (本番)

[Learning] Learning mode set to: PRODUCTION (本番)
[Learning] Production mode: Learning disabled, using existing data only.
```

## 対戦終了時の動作

### 練習モード

```
========================================
[Learning] Game finished! Result: WIN
[Learning] Mode: PRACTICE
[Learning] Total games: 15 (W:9 L:5 D:1)
[Learning] Win rate: 60.00%
[Learning] Applying pattern adjustments (factor=0.0129)...
[Learning] Pattern scores updated:
  FIVE: 100000, OPEN_FOUR: 10050, FOUR: 1020
[Learning] Added winning opening moves to book.
[Learning] Learning data saved successfully.
========================================
```

### 本番モード

```
========================================
[Learning] Game finished! Result: WIN
[Learning] Mode: PRODUCTION
[Learning] Production mode: No learning performed.
[Learning] Current stats - Total games: 15 (W:9 L:5 D:1)
[Learning] Win rate: 60.00%
========================================
```

**重要な違い:**
- 本番モードでは学習処理が実行されない
- 統計情報は表示されるが、データは更新されない
- パターンスコアや序盤定石は変更されない

## 使用シナリオ

### シナリオ1: 新しいAIのトレーニング

```
1. config.txt で LEARNING_MODE=PRACTICE に設定
2. 50-100試合を対戦
3. AIが自動的に学習・成長
4. 学習データが learning_data.dat に保存される
5. 序盤定石が opening_book.csv に追加される
```

### シナリオ2: 大会での使用

```
1. 練習モードで十分にトレーニング済み
2. config.txt で LEARNING_MODE=PRODUCTION に設定
3. 学習済みデータを使用して安定した性能を発揮
4. 対戦中は学習せず、既存の知識のみを使用
5. 予期せぬ学習による性能変動を防止
```

### シナリオ3: 学習効果の評価

```
1. 初期状態で PRACTICE モードで10試合
2. PRODUCTION モードに切り替えて10試合（学習済みデータ使用）
3. 再び PRACTICE モードで10試合（さらに学習）
4. 勝率の推移を比較して学習効果を確認
```

## 学習データファイル

### learning_data.dat
- バイナリ形式の学習データ
- 統計情報（総試合数、勝敗記録）
- パターン学習データ（調整値、成功/失敗回数）
- サイズ: 120バイト

**練習モード**: 更新される
**本番モード**: 読み込みのみ、更新されない

### opening_book.csv
- CSV形式の序盤定石データ
- 勝利した試合の序盤5手を記録

**練習モード**: 勝利時に自動追加
**本番モード**: 読み込みのみ、追加されない

### patterns.csv
- パターンスコアの設定ファイル
- 評価関数で使用するスコア値

**注意**: このファイルは両モードで直接更新されません。
learning_data.dat の調整値がメモリ上で適用されます。

## トラブルシューティング

### Q: 学習モードを変更しても反映されない

**A:** 以下を確認してください：
1. config.txt の LEARNING_MODE の値が正しいか
2. プログラムを再起動したか
3. config.txt の読み込みに成功しているか（起動時のメッセージを確認）

### Q: 本番モードでも学習データが変更される

**A:** 以下を確認してください：
1. 本当に PRODUCTION モードになっているか（起動時のメッセージ確認）
2. learning_data.dat のタイムスタンプが対戦前後で変わっていないか
3. 対戦終了時に "No learning performed" と表示されているか

### Q: 練習モードで学習が進まない

**A:** 以下を確認してください：
1. 対戦が正常に終了しているか（勝敗が確定しているか）
2. 最低5試合以上対戦しているか（MIN_GAMES_FOR_LEARNING=5）
3. 学習データの保存に成功しているか

### Q: 学習データをリセットしたい

**A:** 以下のファイルを削除してください：
```cmd
del learning_data.dat
del opening_book.csv
```
次回起動時に新しいデータで学習が開始されます。

## 実装詳細（開発者向け）

### データ構造

```c
// 学習モード列挙型
typedef enum {
    LEARNING_MODE_PRODUCTION = 0,  // 本番モード
    LEARNING_MODE_PRACTICE = 1     // 練習モード
} LearningMode;

// AI設定構造体
typedef struct {
    int search_depth;
    int tt_size;
    int debug_mode;
    int timeout_ms;
    LearningMode learning_mode;  // 学習モード
} AIConfig;
```

### 主要関数

```c
// 学習モードの設定
void set_learning_mode(LearningMode mode);

// 学習モードの取得
LearningMode get_learning_mode(void);

// 学習モード名の取得
const char* get_learning_mode_name(void);
```

### 学習実行の制御ロジック

```c
void finish_game_record(int result) {
    // 本番モードの場合は学習をスキップ
    if (g_learning_mode == LEARNING_MODE_PRODUCTION) {
        printf("[Learning] Production mode: No learning performed.\n");
        return;  // 早期リターン
    }

    // 練習モードの場合のみ以下を実行
    g_stats.total_games++;
    analyze_game_patterns(result);
    apply_pattern_learning();
    learn_opening_moves(result);
    save_learning_data();
}
```

### 設定ファイル読み込み

```c
// config.c の load_config() 関数
if (strcmp(key, "LEARNING_MODE") == 0) {
    if (strcmp(value, "PRODUCTION") == 0) {
        g_config.learning_mode = LEARNING_MODE_PRODUCTION;
    } else if (strcmp(value, "PRACTICE") == 0) {
        g_config.learning_mode = LEARNING_MODE_PRACTICE;
    } else {
        // デフォルトは練習モード
        g_config.learning_mode = LEARNING_MODE_PRACTICE;
    }
}
```

## まとめ

### 練習モード（PRACTICE）の特徴
✅ 学習する
✅ データ更新する
✅ 成長し続ける
✅ 性能が変動する

**用途**: トレーニング、日常の練習

### 本番モード（PRODUCTION）の特徴
✅ 学習しない
✅ データ更新しない
✅ 安定した性能
✅ 学習済み知識を活用

**用途**: 大会、本番対戦、性能評価

---

**推奨ワークフロー:**
1. **練習期間**: PRACTICE モードで50-100試合をトレーニング
2. **性能評価**: PRODUCTION モードで評価テスト
3. **本番使用**: PRODUCTION モードで安定した性能を発揮
4. **追加学習**: 必要に応じて PRACTICE モードで再トレーニング

この2モードの使い分けにより、学習と実戦のバランスを最適化できます！
