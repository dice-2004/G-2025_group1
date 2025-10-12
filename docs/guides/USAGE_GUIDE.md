# 使い方ガイド

このドキュメントでは、五目並べAI v3.4のビルド、実行、設定方法について説明します。

---

## 🚀 クイックスタート

### Windows

```cmd
# ビルド
build.bat

# 実行
gomoku_new.exe
```

### Linux / Mac / MSYS2

```bash
# ビルド
make all

# 実行
./gomoku

# クリーン
make clean
```

---

## 🔧 ビルド方法

### Windows (build.bat)

**前提条件:**
- GCC（MinGW or MSYS2）がインストールされていること
- `gcc`コマンドがPATHに通っていること

**ビルド手順:**

1. コマンドプロンプトを開く
2. プロジェクトディレクトリに移動
3. `build.bat`を実行

```cmd
cd c:\Users\hddai\oithomes\c\kadai\G-2025_group1
build.bat
```

**生成されるファイル:**
- `gomoku_new.exe` - 実行ファイル
- `*.o` - オブジェクトファイル

### Linux / Mac (Makefile)

**前提条件:**
- GCCがインストールされていること
- makeがインストールされていること

**ビルド手順:**

```bash
# すべてビルド
make all

# 個別ビルド
make gomoku.o          # ゲームインターフェース
make gomoku_ai.o       # AIエントリポイント
make config.o          # 設定管理
make evaluation.o      # 評価関数
make main_engine.o     # Alpha-Beta探索
make transposition.o   # 置換表
make opening_book.o    # 序盤定石
make learning.o        # 学習システム
make TSS.o             # 詰み探索
make debug_utils.o     # デバッグユーティリティ

# クリーン
make clean             # オブジェクトファイルを削除
make clean-all         # すべての生成ファイルを削除
```

詳しくは → [Makefileガイド](./MAKEFILE_GUIDE.md)

---

## 🎮 実行方法

### 基本的な実行

```cmd
# Windows
gomoku_new.exe

# Linux/Mac
./gomoku
```

### コマンドライン引数

現在、コマンドライン引数はサポートされていません。設定は`config.txt`で行います。

---

## ⚙️ 設定方法

### config.txt の編集

`config.txt`ファイルを編集して、AIの動作をカスタマイズできます。

#### 探索深度の変更

```txt
SEARCH_DEPTH=4    # 1-2:初級, 3-4:中級, 5-6:上級, 7-8:大会レベル
```

| 深度 | レベル | 探索時間の目安 |
|-----|--------|---------------|
| 1-2 | 初級 | 瞬時 |
| 3-4 | 中級 | 数秒 |
| 5-6 | 上級 | 10-30秒 |
| 7-8 | 大会レベル | 1分以上 |

#### デバッグレベルの設定

```txt
DEBUG_LEVEL=1     # 0=OFF, 1=基本, 2=詳細, 3=超詳細
```

| レベル | 出力内容 |
|-------|---------|
| 0 | デバッグ出力なし |
| 1 | 基本的な探索情報 |
| 2 | 詳細な評価情報 |
| 3 | すべての探索ノード情報 |

詳しくは → [デバッグ&IDガイド](./DEBUG_AND_ID_GUIDE.md)

#### 学習モードの切り替え

```txt
LEARNING_MODE=PRACTICE      # 学習しながら成長
# または
LEARNING_MODE=PRODUCTION    # 学習済みデータで安定動作
```

| モード | 説明 |
|-------|------|
| PRACTICE | 対戦データから学習し、スコアを調整する |
| PRODUCTION | 学習済みデータを使用し、スコアを変更しない |

詳しくは → [学習モード切替ガイド](./LEARNING_MODE_GUIDE.md)

#### ID時間閾値の調整

```txt
ID_TIME_THRESHOLD=0.8    # 0.7:保守的, 0.8:推奨, 0.9:積極的
```

| 値 | 動作 |
|----|------|
| 0.7 | 保守的（早めに探索を打ち切る） |
| 0.8 | 推奨（バランス良く探索） |
| 0.9 | 積極的（ギリギリまで探索） |

詳しくは → [ID時間管理ガイド](./ID_TIME_MANAGEMENT.md)

---

## 📊 実行中の出力

### 基本的な出力

```
[INFO] Search depth: 4
[INFO] Debug level: 1
[INFO] Learning mode: PRACTICE
[INFO] ID time threshold: 0.80

  A B C D E F G H I J K L M N O
1 . . . . . . . . . . . . . . .
2 . . . . . . . . . . . . . . .
...

Your turn (e.g., H8):
```

### 学習モードの出力

```
[Learning] Game finished! Result: WIN
[Learning] Total games: 12 (W:8 L:4 D:0)
[Learning] Win rate: 66.67%
[Learning] Pattern scores updated:
  OPEN_FOUR: 50250, FOUR: 10150, OPEN_THREE: 5180
[Learning] Learning data saved successfully.
```

### デバッグ出力（DEBUG_LEVEL=1）

```
[ID] Starting depth 1
[ID] Depth 1 completed in 0.05s (score: 150)
[ID] Starting depth 2
[ID] Depth 2 completed in 0.15s (score: 200)
[Search] Nodes: 1234, Trans hits: 567 (45.9%), Best move: H8
```

---

## 📁 ファイル構成

### 設定ファイル

```
config.txt                 ← AI設定ファイル
patterns.csv               ← 基本パターンスコア
patterns_extended.csv      ← 拡張パターンスコア（20個）
opening_book.csv           ← 序盤定石データベース
```

### 学習データファイル

```
learning_data.txt          ← 学習データ（自動生成）
```

### テンプレートファイル

```
templates/
├── example_config.txt              ← 設定例
├── example_learning_output.txt     ← 学習出力例
└── learning_data_format.txt        ← 学習データフォーマット
```

---

## 🐛 トラブルシューティング

### ビルドエラー

**症状:** `gcc: command not found`

**解決方法:**
1. GCCがインストールされているか確認
2. PATHが通っているか確認

**症状:** リンクエラー

**解決方法:**
1. すべての`.o`ファイルが生成されているか確認
2. `make clean`してから再ビルド

### 実行エラー

**症状:** `config.txt not found`

**解決方法:**
1. `config.txt`がカレントディレクトリにあるか確認
2. テンプレートからコピー: `cp templates/example_config.txt config.txt`

**症状:** セグメンテーションフォルト

**解決方法:**
1. デバッグビルド: `make DEBUG=1`
2. デバッグレベルを上げて実行: `DEBUG_LEVEL=2`

---

## 🔍 詳細設定

### パターンスコアのカスタマイズ

`patterns_extended.csv`を編集して、評価パターンのスコアをカスタマイズできます。

```csv
PATTERN_NAME,SCORE,DESCRIPTION
FIVE,100000,五
OPEN_FOUR,50000,開いた四
FOUR,10000,四
OPEN_THREE,5000,開いた三
...
```

詳しくは → [評価パターンガイド](../technical/EVALUATION_PATTERNS.md)

### 序盤定石の追加

`opening_book.csv`を編集して、序盤定石を追加できます。

```csv
MOVE_SEQUENCE,SCORE,COMMENT
H8,100,中央
H7-I8,90,斜め配置
...
```

---

## 関連ドキュメント

- [システム動作完全ガイド](./SYSTEM_BEHAVIOR_GUIDE.md)
- [デバッグ&IDガイド](./DEBUG_AND_ID_GUIDE.md)
- [学習モード切替ガイド](./LEARNING_MODE_GUIDE.md)
- [ID時間管理ガイド](./ID_TIME_MANAGEMENT.md)
- [Makefileガイド](./MAKEFILE_GUIDE.md)
- [機能詳細](./FEATURES.md)
