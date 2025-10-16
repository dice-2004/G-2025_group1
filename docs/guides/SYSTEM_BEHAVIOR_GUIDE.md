# 五目並べAI システム動作説明書 v3.4

## 📋 目次
1. [learning.h の詳細説明](#learningh-の詳細説明)
2. [システム全体の動作フロー](#システム全体の動作フロー)
3. [ファイル入出力の詳細](#ファイル入出力の詳細)
4. [画面表示の説明](#画面表示の説明)
5. [評価パターンの拡張](#評価パターンの拡張)

---

## 📚 learning.h の詳細説明

### 概要
`learning.h`は、対戦を重ねるごとにAIが自動的に強くなる学習システムを定義するヘッダーファイルです。

### 主要な構造体とEnum

#### 1. **LearningMode（学習モード）**
```c
typedef enum {
    LEARNING_MODE_PRODUCTION = 0,  // 本番モード
    LEARNING_MODE_PRACTICE = 1     // 練習モード
} LearningMode;
```

**説明**:
- **PRODUCTION（本番モード）**:
  - 学習データを**参照のみ**
  - スコアは**変更しない**
  - 大会や本番環境で使用
  - 安定した強さを維持

- **PRACTICE（練習モード）**:
  - 学習データを**参照＆更新**
  - 対戦結果からスコアを**自動調整**
  - 練習やトレーニングで使用
  - 対戦するほど強くなる

---

#### 2. **GameRecord（対戦記録）**
```c
typedef struct {
    int move_x[BOARD_SIZE * BOARD_SIZE];      // 各手のX座標
    int move_y[BOARD_SIZE * BOARD_SIZE];      // 各手のY座標
    int move_color[BOARD_SIZE * BOARD_SIZE];  // 各手の色（黒/白）
    int move_count;                            // 手数
    int result;  // 1=勝ち, 0=引き分け, -1=負け
} GameRecord;
```

**説明**:
- 1対戦分の全ての手を記録
- 最大225手（15×15盤面）まで記録可能
- 勝敗結果も保存
- 学習時にこのデータを分析

**使用例**:
```
対戦開始 → start_game_record()
  ↓
手を打つ → record_move(7, 7, BLACK)
  ↓
手を打つ → record_move(7, 8, WHITE)
  ↓
... (対戦続行)
  ↓
対戦終了 → finish_game_record(1)  // 1=勝ち
```

---

#### 3. **LearningStats（学習統計）**
```c
typedef struct {
    int total_games;   // 総対戦数
    int wins;          // 勝利数
    int losses;        // 敗北数
    int draws;         // 引き分け数
    double win_rate;   // 勝率（0.0～1.0）
} LearningStats;
```

**説明**:
- 学習の進捗を数値化
- 勝率でAIの成長を確認
- `get_learning_stats()`で取得可能

---

### 主要関数の説明

#### **init_learning()**
```c
void init_learning(void);
```
- 学習システムの初期化
- メモリの確保
- 統計情報のリセット
- AI起動時に1回だけ呼ばれる

---

#### **start_game_record()**
```c
void start_game_record(void);
```
- 新しい対戦の記録開始
- GameRecordをリセット
- 各対戦の最初に呼ばれる

---

#### **record_move()**
```c
void record_move(int x, int y, int color);
```
- 1手を記録
- 対戦中、手を打つたびに呼ばれる
- 例: `record_move(7, 7, BLACK);`

---

#### **finish_game_record()**
```c
void finish_game_record(int result);
```
- 対戦終了処理
- 学習の実行（PRACTICEモードの場合）
- パターンスコアの自動調整
- 統計情報の更新
- 結果: `1`=勝ち、`0`=引き分け、`-1`=負け

**学習アルゴリズム**:
```
勝利時: 使用したパターンのスコアを +1% 増加
敗北時: 使用したパターンのスコアを -2% 減少
引き分け: スコア変更なし
```

---

#### **save_learning_data()**
```c
void save_learning_data(void);
```
- 学習データを`learning_data.dat`に保存
- バイナリ形式で保存
- AI終了時に呼ばれる
- 次回起動時にデータを引き継ぐ

---

#### **load_learning_data()**
```c
void load_learning_data(void);
```
- 学習データを`learning_data.dat`から読み込み
- AI起動時に呼ばれる
- ファイルがない場合は初期値を使用

---

#### **set_learning_mode() / get_learning_mode()**
```c
void set_learning_mode(LearningMode mode);
LearningMode get_learning_mode(void);
const char* get_learning_mode_name(void);
```
- 学習モードの設定・取得
- `config.txt`から設定を読み込み
- モード名の文字列取得も可能

---

## 🎮 システム全体の動作フロー

### 起動時の処理
```
1. main() 実行
   ↓
2. initAI() 呼び出し
   ↓
3. load_config("config.txt") - 設定読み込み
   ↓
4. load_pattern_scores("patterns.csv") - パターン読み込み
   ↓
5. init_zobrist() - ハッシュテーブル初期化
   ↓
6. init_transposition_table() - 置換表初期化
   ↓
7. load_opening_book("opening_book.csv") - 定石読み込み
   ↓
8. init_learning() - 学習システム初期化
   ↓
9. load_learning_data() - 学習データ読み込み
   ↓
10. set_learning_mode() - 学習モード設定
   ↓
11. 対戦準備完了！
```

### 対戦中の処理
```
1. start_game_record() - 対戦記録開始
   ↓
2. ゲームループ開始
   ↓
   [人間のターン]
   ├─ 座標入力 (例: 7 7)
   ├─ record_move(7, 7, BLACK) - 手を記録
   └─ 盤面更新
   ↓
   [AIのターン]
   ├─ AI() 呼び出し
   │   ├─ find_opening_move() - 定石チェック
   │   ├─ iterative_deepening() または alpha_beta_search()
   │   │   ├─ evaluate_board() - 局面評価
   │   │   ├─ probe_tt() - 置換表検索
   │   │   ├─ generate_moves() - 候補手生成
   │   │   └─ store_tt() - 置換表保存
   │   └─ 最善手決定
   ├─ record_move(x, y, WHITE) - 手を記録
   └─ 盤面更新
   ↓
3. 勝敗判定
   ↓
4. finish_game_record(result) - 対戦終了＆学習
   ↓
5. save_learning_data() - 学習データ保存
```

### 終了時の処理
```
1. finishAI() 呼び出し
   ↓
2. save_learning_data() - 最終保存
   ↓
3. メモリ解放
   ↓
4. プログラム終了
```

---

## 📁 ファイル入出力の詳細

### 入力ファイル

#### 1. **config.txt** - AI設定
```txt
SEARCH_DEPTH=4
TRANSPOSITION_TABLE_SIZE=262144
DEBUG_MODE=0
TIMEOUT_MS=10000
LEARNING_MODE=PRACTICE        ← ここで学習モード設定
ITERATIVE_DEEPENING=1
DEBUG_LEVEL=1
ID_TIME_THRESHOLD=0.8
```

**読み込みタイミング**: AI起動時（initAI内）
**影響**: AI全体の動作を制御

---

#### 2. **patterns.csv / patterns_extended.csv** - パターン評価値
```csv
FIVE,100000,五連（勝利）
OPEN_FOUR,50000,開いた四（次で勝ち）
FOUR,10000,四連（防御必須）
...
```

**読み込みタイミング**: AI起動時
**影響**: 局面評価の基準値

---

#### 3. **opening_book.csv** - 序盤定石
```csv
1,7,7,初手は中央
2,7,8,標準的な応手
3,8,8,三々定石
...
```

**読み込みタイミング**: AI起動時
**影響**: 序盤の手選択

---

#### 4. **learning_data.dat** - 学習データ（バイナリ）
```
[バイナリデータ]
- パターンスコアの調整値
- 対戦統計（勝敗数、勝率）
- 学習済み定石
```

**読み込みタイミング**: AI起動時
**影響**: 学習済みの知識を復元

---

### 出力ファイル

#### 1. **learning_data.dat** - 学習データ保存
**内容**:
```
- 調整されたパターンスコア
  例: OPEN_FOUR: 50000 → 50500 (1%増加)
- 対戦統計
  例: total_games=10, wins=6, losses=4, win_rate=0.60
- 学習した定石
```

**書き込みタイミング**:
- 各対戦終了時（PRACTICEモード）
- AI終了時

**形式**: バイナリ（効率的な保存）

---

#### 2. **opening_book.csv** - 定石追加（学習時）
**追加される内容**:
```csv
[既存の定石]
...
10,6,8,学習した新手
11,9,7,勝利につながった手
```

**書き込みタイミング**: 勝利時に新しい定石を追加

---

## 🖥️ 画面表示の説明

### 起動時の表示

#### 設定読み込み
```
[Config] Loaded from config.txt
  SEARCH_DEPTH = 4
  TT_SIZE = 262144
  DEBUG_MODE = 0
  TIMEOUT_MS = 10000
  LEARNING_MODE = PRACTICE (練習)
  ITERATIVE_DEEPENING = ON
  DEBUG_LEVEL = 1 (基本)
  ID_TIME_THRESHOLD = 0.80 (80%)
```

#### パターン読み込み
```
[Patterns] Loaded 20 patterns from patterns_extended.csv
  FIVE = 100000
  OPEN_FOUR = 50000
  FOUR_FOUR = 45000
  ...
```

#### 定石読み込み
```
[OpeningBook] Loaded 15 moves from opening_book.csv
```

#### 学習データ読み込み
```
[Learning] Loading learning data...
[Learning] Data loaded successfully.
[Learning] Stats: 12 games (W:8 L:4 D:0) - Win rate: 66.67%
[Learning] Mode: PRACTICE (練習モード - 学習する)
```

---

### 対戦中の表示（DEBUG_LEVEL=1の場合）

#### AIの思考開始
```
[AI] Starting search...
[AI] Mode: Iterative Deepening
```

#### 定石使用時
```
[OpeningBook] Using opening move: (7, 7)
[AI] Opening book move selected
```

#### Iterative Deepening実行中
```
========================================
[Iterative Deepening] Starting search
  Max depth: 4
  Time limit: 10000 ms
  Time threshold: 80%
========================================

[ID] Depth 1 completed: (7, 8) score=500 time=0.012s (elapsed: 12ms / 10000ms = 0.1%)
[ID] Depth 2 completed: (7, 8) score=850 time=0.045s (elapsed: 57ms / 10000ms = 0.6%)
[ID] Depth 3 completed: (7, 8) score=1100 time=0.180s (elapsed: 237ms / 10000ms = 2.4%)
[ID] Depth 4 completed: (7, 8) score=1250 time=0.920s (elapsed: 1157ms / 10000ms = 11.6%)

========================================
[ID] Search completed
  Completed depth: 4
  Best move: (7, 8)
  Best score: 1250
  Total time: 1.157 sec
========================================
```

#### 手の決定
```
[AI] Best move: (7, 8) with score 1250
```

---

### 対戦終了時の表示

#### 勝利時（PRACTICEモード）
```
========================================
[Learning] Game finished! Result: WIN
========================================
[Learning] Total games: 13 (W:9 L:4 D:0)
[Learning] Win rate: 69.23%
[Learning] Pattern scores updated:
  OPEN_FOUR: 50000 → 50500 (+1.00%)
  FOUR: 10000 → 10100 (+1.00%)
  OPEN_THREE: 5000 → 5050 (+1.00%)
  THREE: 1000 → 1010 (+1.00%)
[Learning] Learning data saved successfully.
========================================
```

#### 敗北時（PRACTICEモード）
```
========================================
[Learning] Game finished! Result: LOSS
========================================
[Learning] Total games: 14 (W:9 L:5 D:0)
[Learning] Win rate: 64.29%
[Learning] Pattern scores updated:
  OPEN_FOUR: 50500 → 49490 (-2.00%)
  FOUR: 10100 → 9898 (-2.00%)
  OPEN_THREE: 5050 → 4949 (-2.00%)
  THREE: 1010 → 990 (-2.00%)
[Learning] Learning data saved successfully.
========================================
```

#### 本番モードの場合
```
[Learning] Mode: PRODUCTION - No learning performed
[Learning] Total games: 15 (W:9 L:5 D:1)
[Learning] Win rate: 60.00%
```

---

### デバッグレベル別の表示

#### DEBUG_LEVEL=0（出力なし）
```
（通常の対戦メッセージのみ）
```

#### DEBUG_LEVEL=1（基本情報）
```
[AI] Starting search...
[ID] Depth 1 completed: (7, 8) score=500
[AI] Best move: (7, 8)
```

#### DEBUG_LEVEL=2（詳細情報）
```
[Debug] Current Board State:
  A B C D E F G H I J K L M N O
1 · · · · · · · · · · · · · · ·
2 · · · · · · · · · · · · · · ·
...
7 · · · · · · ● ○ · · · · · · ·
...

[Debug Stats]
  Nodes searched: 12450
  Cutoffs: 8920 (71.6%)
  TT hits: 3280 (26.3%)
  Time: 0.920 sec
  Nodes/sec: 13533

[TT] Stats: 1250 queries, 328 hits (26.2%)
```

#### DEBUG_LEVEL=3（超詳細）
```
[Root] Move 1/25: (7,8) score=1250
[Root] Move 2/25: (8,8) score=1100
[Root] Move 3/25: (6,7) score=900
...
[Beta Cutoff] at move (7,9)
[TT Hit] depth=3 score=850
[Alpha Update] alpha=-800 → 1100
```

---

## 🎯 評価パターンの拡張（v3.4）

### 拡張前（8パターン）
```
FIVE, OPEN_FOUR, FOUR, OPEN_THREE, THREE, OPEN_TWO, TWO, ONE
```

### 拡張後（20パターン）

#### 最重要パターン（4個）
```
1. FIVE - 五連（勝利確定）
2. OPEN_FOUR - 開いた四（次で勝ち）
3. FOUR_FOUR - 四四（2つの四が同時）
4. FOUR_THREE - 四三（四と三が同時）
```

#### 準勝ちパターン（5個）
```
5. FOUR - 四連（防御必須）
6. BROKEN_FOUR - 飛び四（●●_●●）
7. OPEN_THREE_THREE - 開いた三×2
8. OPEN_THREE - 開いた三
9. BROKEN_THREE - 飛び三
```

#### 攻撃パターン（5個）
```
10. THREE_THREE - 三×2
11. THREE - 三連
12. OPEN_TWO_TWO - 開いた二×2
13. OPEN_TWO - 開いた二
14. BROKEN_TWO - 飛び二
```

#### 基本パターン（3個）
```
15. TWO_TWO - 二×2
16. TWO - 二連
17. ONE - 一つ
```

#### 特殊パターン（3個）
```
18. CENTER - 中央付近
19. NEAR_ENEMY - 相手の石の近く
20. CONNECTION - 既存の石との連結
```

### パターンの精度向上効果

1. **複合パターンの認識**: 四四、四三など強力な攻撃形を正確に評価
2. **飛びパターンの検出**: 間に空きがあるパターンも評価対象
3. **位置評価の追加**: 中央や連結など戦略的な位置を考慮
4. **より細かい評価**: 8段階 → 20段階の詳細な評価

---

## 📊 学習データの変化例

### 初期状態（1対戦目）
```
Total games: 0
Wins: 0, Losses: 0, Draws: 0
Win rate: 0.00%

Pattern scores:
  FIVE: 100000
  OPEN_FOUR: 50000
  FOUR: 10000
  OPEN_THREE: 5000
  THREE: 1000
```

### 10対戦後（勝率60%）
```
Total games: 10
Wins: 6, Losses: 4, Draws: 0
Win rate: 60.00%

Pattern scores:
  FIVE: 100000 (変化なし)
  OPEN_FOUR: 51000 (+2.00%)
  FOUR: 10200 (+2.00%)
  OPEN_THREE: 5100 (+2.00%)
  THREE: 1020 (+2.00%)
```

### 100対戦後（勝率75%）
```
Total games: 100
Wins: 75, Losses: 20, Draws: 5
Win rate: 75.00%

Pattern scores:
  FIVE: 100000 (変化なし)
  OPEN_FOUR: 57500 (+15.00%)
  FOUR: 11500 (+15.00%)
  OPEN_THREE: 5750 (+15.00%)
  THREE: 1150 (+15.00%)
```

---

## 🎓 まとめ

### learning.hの役割
- 対戦データの記録
- パターンスコアの自動調整
- 学習データの永続化
- 学習モードの管理

### システム全体の動作
1. 起動時に設定とデータを読み込み
2. 対戦中に全ての手を記録
3. 対戦終了時に学習を実行
4. 終了時にデータを保存

### ファイルの役割
- **入力**: config.txt, patterns.csv, opening_book.csv, learning_data.dat
- **出力**: learning_data.dat（更新）

### 表示の内容
- 設定情報、探索状況、学習結果
- DEBUG_LEVELで表示量を調整可能

### 拡張されたパターン
- 8パターン → 20パターン
- より精密な局面評価
- 複合パターンと位置評価の追加

---

**このシステムは、対戦を重ねるごとに自動的に強くなる、完全な自己学習型AIです！** 🎉
