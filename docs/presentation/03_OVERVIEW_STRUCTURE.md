# スライド4: プログラムの概要（構造）

## ファイル構成

### プロジェクト構造

```
G-2025_group1/
│
├── gomoku.c/h                   メインプログラム（変更なし）
├── gomoku_ai.c/h                AIエントリポイント
├── config.c/h                   設定管理
│
├── logics/                      コアロジック
│   ├── main_engine.c/h          Alpha-Beta探索 + ID法
│   ├── evaluation.c/h           評価関数（20パターン）
│   ├── transposition.c/h        置換表（Zobrist Hash）
│   ├── opening_book.c/h         序盤定石
│   ├── learning.c/h             学習システム
│   ├── TSS.c/h                  詰み探索
│   └── debug_utils.c/h          デバッグユーティリティ
│
├── config.txt                   設定ファイル
├── patterns_extended.csv        評価パターンデータ（20種類）
├── opening_book.csv             序盤定石データベース
├── learning_data.dat            学習データ（バイナリ）
│
├── build.bat                    ビルドスクリプト (Windows)
├── Makefile                     ビルドシステム (Linux/Mac)
│
└── docs/                        ドキュメント（48個）
    ├── quickstart/              クイックスタート
    ├── guides/                  使い方ガイド
    ├── technical/               技術ドキュメント
    ├── verification/            検証レポート
    └── reports/                 実装報告書
```

---

## モジュール構成

### 主要モジュールとその役割

| モジュール | 責務 | ファイル |
|----------|------|---------|
| **メイン** | ユーザーI/O、盤面表示 | `gomoku.c/h` |
| **AIエントリ** | AI思考エンジンへの橋渡し | `gomoku_ai.c/h` |
| **設定管理** | CSV設定ファイル読み込み | `config.c/h` |
| **探索エンジン** | Alpha-Beta + ID法 | `main_engine.c/h` |
| **評価関数** | 盤面評価（20パターン） | `evaluation.c/h` |
| **置換表** | 探索結果キャッシュ | `transposition.c/h` |
| **序盤定石** | 定石データベース | `opening_book.c/h` |
| **学習システム** | 自動学習・最適化 | `learning.c/h` |
| **TSS** | 詰み探索 | `TSS.c/h` |
| **デバッグ** | ログ・統計表示 | `debug_utils.c/h` |

---

## データ構造の概要

### 主要なデータ構造

#### 1. ビットボード（盤面表現）

```c
typedef struct {
    uint64_t black[4];  // 黒石のビットマップ
    uint64_t white[4];  // 白石のビットマップ
    uint64_t fill[4];   // 埋まっている位置
} Orientation;

typedef struct {
    Orientation row;       // 横方向
    Orientation col;       // 縦方向
    Orientation diag_dr;   // 右下斜め
    Orientation diag_dl;   // 左下斜め
} AllMaps;
```

**特徴**:
- 15x15 = 225マス → 64bit × 4 = 256bit
- ビット演算で高速処理
- 4方向を独立管理

#### 2. 置換表エントリ

```c
typedef struct {
    uint64_t hash;      // Zobrist Hash値
    int score;          // 評価値
    int depth;          // 探索深度
    int type;           // EXACT/LOWER/UPPER
    int best_x;         // 最善手x座標
    int best_y;         // 最善手y座標
} TTEntry;
```

**特徴**:
- ハッシュ値で盤面を一意識別
- 探索結果をキャッシュ
- 深度情報で有効性判定

#### 3. 学習データ

```c
typedef struct {
    int total_games;      // 総試合数
    int wins;             // 勝利数
    int losses;           // 敗北数
    int draws;            // 引き分け数
    double win_rate;      // 勝率
} LearningStats;

typedef struct {
    double score_adjustment[20];  // 各パターンの累積調整値
    int pattern_success[20];      // 各パターンの成功回数
    int pattern_failure[20];      // 各パターンの失敗回数
} PatternLearningData;
```

**特徴**:
- 対戦統計を記録
- パターンごとの成功/失敗を追跡
- ファイルに永続化

---

## 設定ファイル

### config.txt

```ini
# 探索設定
SEARCH_DEPTH=6              # 探索深さ
ITERATIVE_DEEPENING=1       # ID法を使用（0=無効, 1=有効）
TIMEOUT_MS=5000             # 時間制限（ミリ秒）

# 置換表設定
TT_SIZE=262144              # 置換表サイズ（約4MB）

# デバッグ設定
DEBUG_LEVEL=1               # 0=エラーのみ, 1=情報, 2=詳細, 3=トレース

# 学習設定
LEARNING_MODE=PRODUCTION    # PRACTICE=学習中, PRODUCTION=本番
```

---

## データファイル

### patterns_extended.csv（20パターン）

```csv
# パターン名, スコア, 説明
FIVE,100000,五連（勝利確定）
OPEN_FOUR,50000,両側空き四連（次確実）
FOUR_FOUR,45000,四四の複合形
FOUR_THREE,40000,四三の複合形
FOUR,10000,片側空き四連
BROKEN_FOUR,8000,間空き四連
...
```

**20パターン**:
- 基本: FIVE, OPEN_FOUR, FOUR, OPEN_THREE, THREE, OPEN_TWO, TWO, ONE
- 複合: FOUR_FOUR, FOUR_THREE, THREE_THREE, OPEN_THREE_THREE, TWO_TWO, OPEN_TWO_TWO
- 飛び: BROKEN_FOUR, BROKEN_THREE, BROKEN_TWO
- 位置: CENTER, NEAR_ENEMY, CONNECTION

### opening_book.csv（序盤定石）

```csv
# ゲームID, 手番, x座標, y座標, コメント
1,1,7,7,初手は中央
1,2,7,6,中央に対して上に配置
1,3,8,7,右に展開
...
```

**特徴**:
- ゲーム単位で定石を管理
- 勝利パターンを自動学習
- 11種類の定石が登録済み

### learning_data.dat（学習データ）

- **バイナリ形式**
- **120バイト**
  - LearningStats: 24バイト
  - PatternLearningData: 96バイト
- **起動時に自動読込**
- **対戦終了時に自動保存**

---

## メモリ使用量

### 各データ構造のサイズ

| データ構造 | サイズ | 説明 |
|-----------|--------|------|
| **ビットボード** | 96バイト | 4方向 × 24バイト |
| **置換表** | 4 MB | 262,144エントリ × 16バイト |
| **Zobrist Hash** | 3.6 KB | 15×15×2 × 8バイト |
| **学習データ** | 120バイト | stats + pattern_data |
| **序盤定石** | ~2 KB | 最大250手 |
| **パターンスコア** | 80バイト | 20パターン × 4バイト |
| **合計** | **~4.1 MB** | 非常にコンパクト |

---

## 処理の流れと構造の関係

```
ユーザー入力
  ↓
gomoku.c (メイン)
  ↓
gomoku_ai.c (AIエントリ)
  ↓
config.c (設定読込) ←─ config.txt
  ↓
┌─────────────────────────────┐
│ logics/ (コアロジック)       │
│ ┌─────────────────────────┐ │
│ │ TSS.c (詰み探索)         │ │
│ ├─────────────────────────┤ │
│ │ opening_book.c (定石)    │ │ ← opening_book.csv
│ ├─────────────────────────┤ │
│ │ main_engine.c (探索)     │ │
│ │   ├─ transposition.c    │ │
│ │   └─ evaluation.c       │ │ ← patterns_extended.csv
│ ├─────────────────────────┤ │
│ │ learning.c (学習)        │ │ ← learning_data.dat
│ └─────────────────────────┘ │
└─────────────────────────────┘
  ↓
着手決定
  ↓
learning.c (学習記録) ──→ learning_data.dat
                      ──→ opening_book.csv (勝利時)
```

---

## モジュール間の依存関係

```
gomoku_ai.c
    ├── uses → config.c (設定読み込み)
    ├── uses → evaluation.c (盤面評価)
    ├── uses → main_engine.c (探索エンジン)
    ├── uses → TSS.c (詰み探索)
    ├── uses → opening_book.c (序盤定石)
    ├── uses → transposition.c (置換表)
    └── uses → learning.c (学習システム)

main_engine.c
    ├── uses → evaluation.c (評価)
    ├── uses → transposition.c (キャッシュ)
    └── uses → config.c (パラメータ)

learning.c
    ├── uses → evaluation.c (スコア調整)
    └── uses → opening_book.c (定石追加)
```

**特徴**:
- ✅ **疎結合**: モジュール間の依存が最小限
- ✅ **単一責任**: 各モジュールが1つの機能に集中
- ✅ **拡張容易**: 新機能の追加が簡単

---

## 実装規模

### コード規模

| カテゴリ | 行数 | 説明 |
|---------|------|------|
| **メインロジック** | ~1,500行 | gomoku_ai.c, config.c |
| **探索エンジン** | ~800行 | main_engine.c, transposition.c |
| **評価関数** | ~600行 | evaluation.c |
| **学習システム** | ~400行 | learning.c |
| **TSS** | ~1,000行 | TSS.c |
| **その他** | ~200行 | opening_book.c, debug_utils.c |
| **合計** | **~4,500行** | C言語（コメント含む） |

### ドキュメント規模

| カテゴリ | ファイル数 | 推定ページ数 |
|---------|-----------|-------------|
| **クイックスタート** | 3個 | ~100ページ |
| **ガイド** | 10個 | ~300ページ |
| **技術資料** | 7個 | ~1,500ページ |
| **検証レポート** | 13個 | ~600ページ |
| **実装報告** | 15個 | ~700ページ |
| **合計** | **48個** | **~3,200ページ** |

---

## まとめ: プログラムの構造

### 設計の特徴

1. **モジュール分割**
   - 各機能が独立したモジュール
   - 疎結合で拡張容易

2. **データ駆動**
   - CSV設定ファイル
   - 外部からパラメータ変更可能

3. **効率的なデータ構造**
   - ビットボード（96バイト）
   - 置換表（4MB）
   - 合計 ~4.1MB（非常にコンパクト）

4. **包括的なドキュメント**
   - 48個のドキュメント
   - 約3,200ページ
   - 初心者〜研究者まで対応

### 次のスライドで説明すること

各技術要素の詳細:
- 4方向bitmap
- TSS
- 序盤定石
- ID法
- αβ法
- 評価関数（テンプレートマッチング）
- 学習システム
