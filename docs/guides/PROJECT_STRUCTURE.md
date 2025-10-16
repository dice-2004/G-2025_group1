# プロジェクト構成ガイド

このドキュメントでは、五目並べAI v3.4のプロジェクト構成とファイル配置について説明します。

---

## 📁 ディレクトリ構造

```
G-2025_group1/
├── README.md                       ← プロジェクトトップ（このファイルへのリンク）
│
├── docs/                           ← 📚 ドキュメントフォルダ（28個）
│   ├── README.md                   ← ドキュメントトップ
│   │
│   ├── guides/                     ← 📘 ガイドドキュメント（8個）
│   │   ├── README.md
│   │   ├── PROJECT_OVERVIEW.md     ← プロジェクト概要
│   │   ├── USAGE_GUIDE.md          ← 使い方ガイド
│   │   ├── FEATURES.md             ← 機能詳細
│   │   ├── PROJECT_STRUCTURE.md    ← このファイル
│   │   ├── SYSTEM_BEHAVIOR_GUIDE.md
│   │   ├── DEBUG_AND_ID_GUIDE.md
│   │   ├── LEARNING_MODE_GUIDE.md
│   │   ├── ID_TIME_MANAGEMENT.md
│   │   └── MAKEFILE_GUIDE.md
│   │
│   ├── technical/                  ← 📗 技術ドキュメント（5個）
│   │   ├── README.md
│   │   ├── TECHNICAL_DOCUMENT.md
│   │   ├── AI_IMPLEMENTATION.md
│   │   ├── LEARNING_GUIDE.md
│   │   ├── EVALUATION_PATTERNS.md  ← 評価パターンガイド
│   │   └── ZOBRIST_HASHING.md
│   │
│   ├── verification/               ← 📙 検証・最適化レポート（6個）
│   │   ├── README.md
│   │   ├── WINDOW_SIZE_ANALYSIS.md
│   │   ├── IMPLEMENTATION_REPORT_11x6.md
│   │   ├── TEMPLATE_MATCHING_ANALYSIS.md
│   │   ├── OPTIMIZATION_RESULTS.md
│   │   ├── ROLLBACK_REPORT.md
│   │   └── CATEGORY_ORGANIZATION_REPORT.md
│   │
│   ├── reports/                    ← 📕 実装報告書（11個）
│   │   ├── README.md
│   │   ├── IMPLEMENTATION_VERIFICATION.md
│   │   ├── v3.0/
│   │   ├── v3.1/
│   │   ├── v3.2/
│   │   ├── v3.3/
│   │   └── v3.4/
│   │
│   └── quickstart/                 ← 🚀 クイックスタート（2個）
│       ├── README.md
│       ├── QUICKSTART.md
│       └── PRESENTATION_SLIDES.md
│
├── templates/                      ← テンプレートファイル
│   ├── example_config.txt
│   ├── example_learning_output.txt
│   └── learning_data_format.txt
│
├── logics/                         ← AI思考エンジン
│   ├── main_engine.c/h             ← Alpha-Beta探索、Iterative Deepening
│   ├── evaluation.c/h              ← 評価関数（20パターン、11x6窓）
│   ├── transposition.c/h           ← 置換表（Zobrist Hashing）
│   ├── opening_book.c/h            ← 序盤定石
│   ├── learning.c/h                ← 学習システム
│   ├── TSS.c/h                     ← 詰み探索
│   └── debug_utils.c/h             ← デバッグユーティリティ
│
├── gomoku.c/h                      ← ゲームインターフェース
├── gomoku_ai.c/h                   ← AIエントリポイント
├── config.c/h                      ← 設定ファイル管理
│
├── config.txt                      ← AI設定ファイル
├── patterns.csv                    ← 基本パターンスコア
├── patterns_extended.csv           ← 拡張パターンスコア（20個）
├── opening_book.csv                ← 序盤定石データベース
├── learning_data.txt               ← 学習データ（自動生成）
│
├── build.bat                       ← Windows用ビルドスクリプト
├── Makefile                        ← Linux/Mac/MSYS2用ビルドスクリプト
│
├── gomoku_new.exe                  ← Windows用実行ファイル
├── gomoku.exe                      ← 元のゲーム実行ファイル
├── *.o                             ← オブジェクトファイル
│
├── LICENSE                         ← MITライセンス
└── gomoku.pdf                      ← 元の仕様書
```

---

## 📚 ドキュメント構成

### docs/guides/ - ガイドドキュメント（8個）

**用途:** システムの使い方、設定方法、機能の詳細ガイド

| ファイル | 説明 |
|---------|------|
| `PROJECT_OVERVIEW.md` | プロジェクト概要、主要機能、技術的特徴 |
| `USAGE_GUIDE.md` | ビルド、実行、設定方法 |
| `FEATURES.md` | すべての機能の詳細説明 |
| `PROJECT_STRUCTURE.md` | このファイル - プロジェクト構成 |
| `SYSTEM_BEHAVIOR_GUIDE.md` | システムの動作、モード、機能の説明 |
| `DEBUG_AND_ID_GUIDE.md` | デバッグ機能、Iterative Deepening |
| `LEARNING_MODE_GUIDE.md` | 学習モードの切り替え |
| `ID_TIME_MANAGEMENT.md` | ID時間管理機能 |
| `MAKEFILE_GUIDE.md` | Makefileの使い方 |

### docs/technical/ - 技術ドキュメント（5個）

**用途:** アルゴリズム、データ構造、実装の詳細解説

| ファイル | 説明 |
|---------|------|
| `TECHNICAL_DOCUMENT.md` | 完全技術資料（1,200行） |
| `AI_IMPLEMENTATION.md` | Alpha-Beta探索、置換表の実装 |
| `LEARNING_GUIDE.md` | 学習アルゴリズムの詳細 |
| `EVALUATION_PATTERNS.md` | 20種類の評価パターンの詳細 |
| `ZOBRIST_HASHING.md` | Zobrist Hashingの実装 |

### docs/verification/ - 検証・最適化レポート（6個）

**用途:** 評価関数の最適化、窓サイズの分析、正確性の検証

| ファイル | 説明 |
|---------|------|
| `WINDOW_SIZE_ANALYSIS.md` | 窓サイズ詳細分析（提案1/2/3） |
| `IMPLEMENTATION_REPORT_11x6.md` | 11マス抽出+6マス窓の実装報告 |
| `TEMPLATE_MATCHING_ANALYSIS.md` | テンプレートマッチングの正確性分析 |
| `OPTIMIZATION_RESULTS.md` | 最適化試行の結果 |
| `ROLLBACK_REPORT.md` | ロールバックの詳細 |
| `CATEGORY_ORGANIZATION_REPORT.md` | ドキュメントカテゴリ分け報告 |

### docs/reports/ - 実装報告書（11個）

**用途:** 各バージョンの実装完了報告、検証レポート

| ファイル | 説明 |
|---------|------|
| `IMPLEMENTATION_VERIFICATION.md` | 実装検証の総括 |
| `v3.0/` | v3.0実装報告（学習機能） |
| `v3.1/` | v3.1実装報告（学習モード切替） |
| `v3.2/` | v3.2実装報告（デバッグ機能） |
| `v3.3/` | v3.3実装報告（ID時間管理） |
| `v3.4/` | v3.4実装報告（評価パターン拡張） |

### docs/quickstart/ - クイックスタート（2個）

**用途:** すぐに始める方法、プロジェクト概要

| ファイル | 説明 |
|---------|------|
| `QUICKSTART.md` | クイックスタートガイド |
| `PRESENTATION_SLIDES.md` | プロジェクト概要（プレゼン形式） |

---

## 🔧 ソースコード構成

### ゲームインターフェース

```
gomoku.c/h
├── ユーザー入出力
├── 盤面表示
├── ゲームループ
└── 禁じ手判定
```

### AIエントリポイント

```
gomoku_ai.c/h
└── AI思考エンジンへの橋渡し
```

### 設定管理

```
config.c/h
├── config.txt 読み込み
├── patterns.csv 読み込み
├── patterns_extended.csv 読み込み
└── opening_book.csv 読み込み
```

### AI思考エンジン（logics/）

#### main_engine.c/h
```
Alpha-Beta探索
├── Minimax探索の最適化版
├── Alpha-Beta枝刈り
└── Iterative Deepening
```

#### evaluation.c/h
```
評価関数
├── 20種類の評価パターン
├── 11マス抽出
└── 6マス窓スライディング
```

#### transposition.c/h
```
置換表
├── Zobrist Hashing
├── 盤面のハッシュ化
└── 探索結果のキャッシュ
```

#### opening_book.c/h
```
序盤定石
├── データベース検索
└── 定石の適用
```

#### learning.c/h
```
学習システム
├── 勝敗分析
├── パターン評価
├── スコア調整
├── 定石学習
└── データ保存
```

#### TSS.c/h
```
詰み探索
└── 必勝手順の探索
```

#### debug_utils.c/h
```
デバッグユーティリティ
├── デバッグ出力
└── 探索統計
```

---

## ⚙️ 設定ファイル構成

### config.txt - AI設定

```txt
SEARCH_DEPTH=4              # 探索深度
DEBUG_LEVEL=1               # デバッグレベル
LEARNING_MODE=PRACTICE      # 学習モード
ID_TIME_THRESHOLD=0.8       # ID時間閾値
```

詳しくは → [使い方ガイド](./USAGE_GUIDE.md#設定方法)

### patterns.csv - 基本パターンスコア

```csv
PATTERN_NAME,SCORE,DESCRIPTION
FIVE,100000,五
OPEN_FOUR,50000,開いた四
FOUR,10000,四
...
```

### patterns_extended.csv - 拡張パターンスコア

```csv
PATTERN_NAME,SCORE,DESCRIPTION
DOUBLE_FOUR,45000,四四
FOUR_THREE,15000,四三
JUMP_FOUR,8000,飛び四
CENTER_BONUS,100,中央評価
...
```

詳しくは → [評価パターンガイド](../technical/EVALUATION_PATTERNS.md)

### opening_book.csv - 序盤定石

```csv
MOVE_SEQUENCE,SCORE,COMMENT
H8,100,中央
H7-I8,90,斜め配置
...
```

### learning_data.txt - 学習データ（自動生成）

```txt
total_games=12
wins=8
losses=4
draws=0
OPEN_FOUR=50250
FOUR=10150
...
```

---

## 🛠️ ビルドスクリプト

### build.bat - Windows用

```batch
@echo off
echo Building Gomoku AI...

gcc -c gomoku.c -o gomoku.o
gcc -c gomoku_ai.c -o gomoku_ai.o
gcc -c config.c -o config.o
gcc -c logics/evaluation.c -o logics/evaluation.o
...
gcc gomoku.o gomoku_ai.o config.o ... -o gomoku_new.exe

echo Build completed!
```

### Makefile - Linux/Mac/MSYS2用

```makefile
CC = gcc
CFLAGS = -Wall -O2
TARGET = gomoku

all: $(TARGET)

$(TARGET): gomoku.o gomoku_ai.o config.o ...
	$(CC) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o logics/*.o

clean-all: clean
	rm -f $(TARGET)
```

詳しくは → [Makefileガイド](./MAKEFILE_GUIDE.md)

---

## 📦 テンプレートファイル

### templates/example_config.txt

設定ファイルの例。新規プロジェクトでコピーして使用。

### templates/example_learning_output.txt

学習モードの出力例。学習機能のテストに使用。

### templates/learning_data_format.txt

学習データのフォーマット説明。カスタマイズ時に参照。

---

## 🔍 ファイルサイズ目安

| カテゴリ | ファイル数 | 総行数（概算） |
|---------|-----------|--------------|
| ドキュメント | 28個 | ~2,900ページ |
| ソースコード | 14ファイル | ~3,000行 |
| 設定ファイル | 4個 | ~100行 |
| ビルドスクリプト | 2個 | ~50行 |

---

## 📊 依存関係

### ビルド時の依存関係

```
gomoku_new.exe
├── gomoku.o
├── gomoku_ai.o
├── config.o
└── logics/
    ├── main_engine.o
    ├── evaluation.o
    ├── transposition.o
    ├── opening_book.o
    ├── learning.o
    ├── TSS.o
    └── debug_utils.o
```

### 実行時の依存関係

```
gomoku_new.exe
├── config.txt              （必須）
├── patterns.csv            （必須）
├── patterns_extended.csv   （必須）
├── opening_book.csv        （オプション）
└── learning_data.txt       （自動生成）
```

---

## 🔗 ファイル間の関係

### include関係

```
gomoku.c
└── gomoku.h

gomoku_ai.c
├── gomoku_ai.h
└── logics/main_engine.h

config.c
└── config.h

logics/main_engine.c
├── main_engine.h
├── evaluation.h
├── transposition.h
├── opening_book.h
├── learning.h
├── TSS.h
└── debug_utils.h

logics/evaluation.c
└── evaluation.h

...（以下同様）
```

---

## 関連ドキュメント

- [プロジェクト概要](./PROJECT_OVERVIEW.md)
- [使い方ガイド](./USAGE_GUIDE.md)
- [機能詳細](./FEATURES.md)
- [完全技術資料](../technical/TECHNICAL_DOCUMENT.md)
- [Makefileガイド](./MAKEFILE_GUIDE.md)
