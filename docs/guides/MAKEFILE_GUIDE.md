# Makefile 使用ガイド - 五目並べAI v3.3

## 📋 概要

このMakefileは、五目並べAI v3.3をビルドするためのビルドシステムです。
Linux、macOS、MSYS2/MinGWなどのUNIX互換環境で動作します。

---

## 🚀 クイックスタート

### 基本的なビルド

```bash
# ビルド
make all

# または単に
make
```

### 実行

```bash
# Linux/Mac/MSYS2
./gomoku

# Windows (WSL)
./gomoku
```

---

## 📖 Makeコマンド一覧

### `make all` または `make`
プロジェクト全体をビルドします。

**出力**: `gomoku` 実行ファイル

**例**:
```bash
$ make all
Compiling .libgomoku.c...
Compiling gomoku.c...
Compiling gomoku_ai.c...
Compiling config.c...
Compiling logics/evaluation.c...
Compiling logics/transposition.c...
Compiling logics/opening_book.c...
Compiling logics/learning.c...
Compiling logics/main_engine.c...
Compiling logics/TSS.c...
Compiling logics/debug_utils.c...
Linking gomoku...
Build completed successfully!
Executable: gomoku
```

---

### `make clean`
ビルド生成物（オブジェクトファイル、実行ファイル）を削除します。

**削除されるファイル**:
- `gomoku` (実行ファイル)
- `gomoku.exe` (Windows実行ファイル)
- `*.o` (オブジェクトファイル)
- `logics/*.o` (logicsディレクトリのオブジェクトファイル)

**例**:
```bash
$ make clean
Cleaning up...
rm -f gomoku .libgomoku.o gomoku.o gomoku_ai.o config.o ...
rm -f gomoku.exe *.o
rm -f logics/*.o
Clean completed.
```

---

### `make distclean`
ビルド生成物に加えて、**学習データも削除**します。

**削除されるファイル**:
- `make clean`で削除されるすべてのファイル
- `learning_data.dat` (学習データファイル)

**例**:
```bash
$ make distclean
Cleaning up...
...
Clean completed.
Removing learning data...
rm -f learning_data.dat
Full clean completed.
```

**注意**: 学習データが失われるので、慎重に使用してください！

---

### `make rebuild`
クリーンしてから再ビルドします。

**内部動作**: `make clean` → `make all`

**例**:
```bash
$ make rebuild
Cleaning up...
...
Clean completed.
Compiling .libgomoku.c...
...
Build completed successfully!
```

---

### `make help`
ヘルプメッセージを表示します。

**例**:
```bash
$ make help
Gomoku AI Build System v3.3
============================
Usage:
  make all       - Build the program
  make clean     - Remove all build artifacts
  make distclean - Remove build artifacts and learning data
  make rebuild   - Clean and rebuild
  make help      - Show this help message

Features:
  - Learning system (PRACTICE/PRODUCTION mode)
  - Iterative Deepening with time management
  - Debug utilities (DEBUG_LEVEL 0-3)
  - Configurable via config.txt
```

---

## 🔧 ビルド設定

### コンパイラ設定

```makefile
CC = gcc
CFLAGS = -Wall -O2 -std=c99
LDFLAGS = -lm
```

| オプション | 説明 |
|----------|------|
| `-Wall` | すべての警告を有効化 |
| `-O2` | 最適化レベル2（速度重視） |
| `-std=c99` | C99標準に準拠 |
| `-lm` | 数学ライブラリをリンク |

### 最適化レベルの変更

より高速な実行ファイルが必要な場合：

```makefile
CFLAGS = -Wall -O3 -std=c99  # O2 → O3に変更
```

デバッグ情報を含める場合：

```makefile
CFLAGS = -Wall -g -O0 -std=c99  # デバッグ用
```

---

## 📁 コンパイル対象ファイル

### メインモジュール
```
.libgomoku.c      → .libgomoku.o     # ゲームロジック
gomoku.c          → gomoku.o         # エントリポイント
gomoku_ai.c       → gomoku_ai.o      # AI統合
config.c          → config.o         # 設定管理
```

### logicsモジュール
```
logics/evaluation.c    → logics/evaluation.o     # 評価関数
logics/transposition.c → logics/transposition.o  # 置換表
logics/opening_book.c  → logics/opening_book.o   # 序盤定石
logics/learning.c      → logics/learning.o       # 学習システム
logics/main_engine.c   → logics/main_engine.o    # Alpha-Beta探索
logics/TSS.c           → logics/TSS.o            # 詰み探索
logics/debug_utils.c   → logics/debug_utils.o    # デバッグユーティリティ
```

---

## 🐛 トラブルシューティング

### 問題1: `make: command not found`

**原因**: makeがインストールされていない

**解決策**:

#### Windows (MSYS2)
```bash
pacman -S make
```

#### Ubuntu/Debian
```bash
sudo apt-get install build-essential
```

#### macOS
```bash
xcode-select --install
```

---

### 問題2: `gcc: command not found`

**原因**: gccがインストールされていない

**解決策**:

#### Windows (MSYS2)
```bash
pacman -S gcc
```

#### Ubuntu/Debian
```bash
sudo apt-get install gcc
```

#### macOS
```bash
xcode-select --install
```

---

### 問題3: アーキテクチャの不一致エラー

**エラー例**:
```
i386 architecture of input file `gomoku.o' is incompatible with i386:x86-64 output
```

**原因**: 32ビットと64ビットのオブジェクトファイルが混在

**解決策**:
```bash
# 古いオブジェクトファイルを削除して再ビルド
make clean
make all
```

---

### 問題4: undefined reference エラー

**エラー例**:
```
undefined reference to `gameEndProcess'
undefined reference to `_printf'
```

**原因**:
- 必要なファイルがコンパイルされていない
- リンク順序が間違っている

**解決策**:
1. `.libgomoku.c`がOBJSに含まれているか確認
2. `make clean && make all`で完全再ビルド
3. Makefileの`OBJS`変数を確認

---

### 問題5: 警告が出る

**警告例**:
```
warning: format '%u' expects argument of type 'unsigned int', but argument 2 has type 'uint64_t'
```

**対処**:
- これらは**警告**であり、実行には影響しません
- 実行ファイルは正常に生成されます
- 気になる場合は該当箇所のフォーマット指定子を修正

---

## 🔍 依存関係

### ヘッダーファイルの依存関係

```
gomoku.h
  ├── .libgomoku.c
  ├── gomoku.c
  ├── gomoku_ai.h
  │   ├── gomoku_ai.c
  │   ├── logics/evaluation.c
  │   ├── logics/transposition.c
  │   ├── logics/opening_book.c
  │   ├── logics/learning.c
  │   ├── logics/main_engine.c
  │   ├── logics/TSS.c
  │   └── logics/debug_utils.c
  └── config.h
      ├── config.c
      └── gomoku_ai.c
```

Makefileは自動的にこれらの依存関係を処理します。

---

## 💡 使用例

### 例1: 通常のビルド

```bash
$ cd G-2025_group1
$ make
Compiling .libgomoku.c...
...
Build completed successfully!
Executable: gomoku
$ ./gomoku
```

---

### 例2: クリーンビルド

```bash
$ make clean
Cleaning up...
Clean completed.
$ make
Compiling .libgomoku.c...
...
Build completed successfully!
```

---

### 例3: 完全クリーン（学習データも削除）

```bash
$ make distclean
Cleaning up...
...
Removing learning data...
Full clean completed.
$ make
Compiling .libgomoku.c...
...
Build completed successfully!
```

---

### 例4: ワンライナーでクリーン＆ビルド

```bash
$ make rebuild
Cleaning up...
...
Build completed successfully!
```

---

## 🎯 ビルドフロー

```
make all
  ↓
各.cファイルをコンパイル (.o生成)
  ├── .libgomoku.c → .libgomoku.o
  ├── gomoku.c → gomoku.o
  ├── gomoku_ai.c → gomoku_ai.o
  ├── config.c → config.o
  ├── logics/evaluation.c → logics/evaluation.o
  ├── logics/transposition.c → logics/transposition.o
  ├── logics/opening_book.c → logics/opening_book.o
  ├── logics/learning.c → logics/learning.o
  ├── logics/main_engine.c → logics/main_engine.o
  ├── logics/TSS.c → logics/TSS.o
  └── logics/debug_utils.c → logics/debug_utils.o
  ↓
すべての.oファイルをリンク
  ↓
実行ファイル `gomoku` を生成
  ↓
完了！
```

---

## 📊 ビルド時間の目安

| 環境 | 初回ビルド | 再ビルド（変更なし） |
|-----|----------|-------------------|
| 高速PC | 2-3秒 | < 1秒 |
| 通常PC | 5-10秒 | 1-2秒 |
| 低速PC | 15-30秒 | 3-5秒 |

**注**: ファイルを1つだけ変更した場合、そのファイルと依存ファイルのみ再コンパイルされるため、高速です。

---

## 🔧 カスタマイズ

### ビルドオプションの追加

デバッグ情報を含める：
```makefile
CFLAGS = -Wall -O2 -std=c99 -g
```

さらに詳細な警告：
```makefile
CFLAGS = -Wall -Wextra -O2 -std=c99
```

32ビットビルド：
```makefile
CFLAGS = -Wall -O2 -std=c99 -m32
LDFLAGS = -lm -m32
```

---

## 📝 まとめ

### よく使うコマンド

```bash
make           # ビルド
make clean     # クリーン
make rebuild   # クリーン＆ビルド
make help      # ヘルプ
```

### ビルドが成功したら

```bash
./gomoku       # 実行
```

### トラブル時の対処

```bash
make clean     # まずクリーン
make all       # 再ビルド
```

---

**Makefile バージョン**: v3.3
**対応プロジェクト**: 五目並べAI v3.3
**更新日**: 2025年10月11日
**ステータス**: ✅ 完全動作確認済み
