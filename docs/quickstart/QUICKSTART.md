# 五目並べAI - クイックスタートガイド

## 概要

このプロジェクトは、大会レベルを目指した高性能な五目並べAI実装です。

## 主要機能

✅ **ビットボード**: 高速な盤面表現
✅ **Alpha-Beta探索**: 先読みによる最善手選択
✅ **Zobrist Hashing + 置換表**: 探索の高速化
✅ **序盤定石**: オープニングブックによる序盤の安定化
✅ **テンプレートマッチング**: パターン認識による盤面評価（CSV設定可能）
✅ **TSS（詰み探索）**: 即座に勝敗が決する手の高速判定
✅ **禁じ手判定**: 黒番の三三・四四・長連の完全チェック
✅ **設定ファイル**: 探索深度やパラメータを簡単に変更可能
✅ **学習機能**: 対戦するほど強くなる自動学習システム 🆕

## ビルド方法

### Windows（推奨）

```cmd
build.bat
```

成功すると `gomoku_new.exe` が生成されます。

### Linux/Mac

```bash
gcc -Wall -O3 -c gomoku_ai.c -o gomoku_ai.o
gcc -Wall -O3 -c logics/evaluation.c -o logics/evaluation.o
gcc -Wall -O3 -c logics/main_engine.c -o logics/main_engine.o
gcc -Wall -O3 -c logics/TSS.c -o logics/TSS.o
gcc -Wall -O3 -c .libgomoku.c -o .libgomoku.o
gcc -Wall -O3 -c gomoku.c -o gomoku.o
gcc gomoku.o gomoku_ai.o .libgomoku.o logics/evaluation.o logics/main_engine.o logics/TSS.o -o gomoku
```

## 実行方法

```cmd
gomoku_new.exe
```

または（Linux/Mac）:
```bash
./gomoku
```

## 設定のカスタマイズ

### 探索深度の変更方法 ⭐重要⭐

AIの強さと思考時間は `config.txt` で簡単に変更できます：

```txt
# config.txt を編集
SEARCH_DEPTH=4    ← この数値を変更

# 推奨設定:
# 深さ1-2: 瞬時（初級レベル）
# 深さ3-4: 1秒前後（中級レベル）★デフォルト
# 深さ5-6: 5-10秒（上級レベル）
# 深さ7-8: 30-60秒（大会レベル）
```

### パターンスコアの調整

`patterns.csv` を編集して、評価値を変更できます：

```csv
# patterns.csv
FIVE,100000,五連（勝利確定）
OPEN_FOUR,50000,開いた四
FOUR,10000,四連
OPEN_THREE,5000,開いた三  ← スコアを変更可能
...
```

### 序盤定石の追加

`opening_book.csv` に好みの序盤手順を追加できます：

```csv
# opening_book.csv
1,7,7,初手は中央
2,8,8,黒の斜め
3,6,6,白の対角  ← 新しい定石を追加
```

## 学習機能について 🆕

**対戦するほど強くなります！**

AIは対戦を重ねるごとに自動的に学習します：

- **パターンスコア自動調整**: 勝敗に応じて評価関数を最適化
- **序盤定石の学習**: 勝った試合の序盤手を自動記憶
- **学習データの永続化**: 学習内容を `learning_data.dat` に保存

```
========================================
[Learning] Game finished! Result: WIN
[Learning] Total games: 12 (W:8 L:4 D:0)
[Learning] Win rate: 66.67%
[Learning] Applying pattern adjustments...
[Learning] Pattern scores updated:
  FIVE: 100000, OPEN_FOUR: 50250, FOUR: 10150
  OPEN_THREE: 5180, THREE: 1090, OPEN_TWO: 520
[Learning] Added winning opening moves to book.
[Learning] Learning data saved successfully.
========================================
```

詳しくは **`LEARNING_GUIDE.md`** をご覧ください。

### 学習のリセット

```cmd
# 学習データを削除して最初からやり直す
del learning_data.dat
```

## AIの動作ログ例

```
[Config] Loaded from config.txt
  SEARCH_DEPTH = 4
  TT_SIZE = 262144
[Patterns] Loaded 8 patterns from patterns.csv
[OpeningBook] Loaded 7 moves from opening_book.csv
[Zobrist] Initialized hash table
[TT] Initialized with 262144 entries (~4.0 MB)
[Learning] Learning system initialized.
[Learning] Loaded previous learning data:
  Total games: 5 (W:3 L:2 D:0)
  Win rate: 60.00%
[AI] Initialization complete.

[AI] Move 1 thinking...
[OpeningBook] Found move 1: (7,7) - 初手は中央

[AI] Move 3 thinking...
[TSS] Threat detected at (9, 9), must defend!

[AI] Move 5 thinking...
[Alpha-Beta] Searching 45 moves at depth 4...
[Alpha-Beta] Best move: (8, 8) with score 2340
[TT] Stats: 1523 queries, 891 hits (58.5%)
```

## ファイル構成

```
.
├── gomoku_new.exe          # 実行ファイル（ビルド後）
├── build.bat               # ビルドスクリプト（Windows）
├── config.txt              # ⭐設定ファイル（探索深度など）
├── patterns.csv            # ⭐パターンスコア定義
├── opening_book.csv        # ⭐序盤定石データベース
├── learning_data.dat       # 🆕学習データ（自動生成）
├── AI_IMPLEMENTATION.md    # 詳細な実装ドキュメント
├── LEARNING_GUIDE.md       # 🆕学習機能ガイド
│
├── config.c/h              # 設定読み込みモジュール
├── gomoku.c/h              # メインプログラム
├── gomoku_ai.c/h           # AIエントリポイント + ビットボード
└── logics/
    ├── evaluation.c/h      # 評価関数（CSV対応）
    ├── transposition.c/h   # 置換表（Zobrist Hashing）
    ├── opening_book.c/h    # 序盤定石
    ├── learning.c/h        # 🆕学習システム
    ├── main_engine.c/h     # Alpha-Beta探索
    └── TSS.c/h             # 詰み探索
```

## 性能

- **探索速度**: 深さ4で約1秒/手
- **メモリ使用**: 約200KB
- **推定棋力**: 上級レベル

## 詳細ドキュメント

詳しい実装の説明は `AI_IMPLEMENTATION.md` を参照してください。

## 実装済み機能

- [x] ビットボード
- [x] Alpha-Beta探索
- [x] Zobrist Hashing + 置換表
- [x] 序盤定石データベース
- [x] 評価関数（CSV設定可能）
- [x] TSS（詰み探索）
- [x] 禁じ手判定
- [x] 設定ファイルによる調整機能
- [x] 学習機能（対戦するほど強くなる） 🆕

## さらなる改善案

- [ ] 反復深化探索の時間制御
- [ ] より高度な手順序付け
- [ ] VCF/VCT探索
- [ ] ニューラルネットワークの統合

## ライセンス

MIT License

## 更新履歴

- 2025-10-11 v3.0: 学習機能実装 🆕
  - 対戦するほど強くなる自動学習システム
  - パターンスコアの動的調整
  - 序盤定石の自動追加
  - 学習データの永続化
- 2025-10-11 v2.0: ステップ4-5完成
  - Zobrist Hashing + 置換表
  - 序盤定石データベース
  - CSV設定ファイル対応
  - 探索深度の簡単変更
- 2025-10-11 v1.0: 初版リリース（ステップ1～3実装完了）
