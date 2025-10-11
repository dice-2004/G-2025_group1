# 五目並べAI 実装ドキュメント

## 概要

本プロジェクトは、大会レベルを目指した高性能な五目並べAIです。以下の技術を統合して実装されています：

- **ビットボード**: 高速な盤面表現
- **テンプレートマッチング評価関数**: パターン認識による盤面評価
- **Alpha-Beta法**: 効率的な先読み探索
- **TSS（Threat Space Search）**: 詰み・必死の高速判定
- **禁じ手判定**: 黒番の三三・四四・長連の完全チェック

## ファイル構成

```
.
├── gomoku.c              # メインプログラム（外部提供）
├── gomoku.h              # 共通ヘッダー
├── gomoku_ai.c           # AIエントリポイント
├── gomoku_ai.h           # ビットボード定義とヘルパー関数
└── logics/
    ├── evaluation.c/h    # 評価関数（テンプレートマッチング）
    ├── main_engine.c/h   # Alpha-Beta探索エンジン
    └── TSS.c/h           # 詰み探索
```

## モジュール説明

### 1. ビットボード（gomoku_ai.c/h）

**役割**: 盤面を高速に扱うためのビット演算ベースの表現

**主要機能**:
- `build_maps()`: int配列からビットボードへの変換（4方向）
  - row-major（横方向）
  - col-major（縦方向）
  - diag-dr（右下斜め）
  - diag-dl（左下斜め）
- `place_stone()`, `remove_stone()`: ビットボード上での石の操作
- `generate_legal_moves()`: 合法手生成（禁じ手除外済み）
- `is_forbidden()`: 黒番の禁じ手判定（三三・四四・長連）

**最適化ポイント**:
- uint64_t配列でビット管理
- 斜め方向は変換テーブル使用
- 合法手候補は既存石の周辺2マスに限定

### 2. 評価関数（logics/evaluation.c/h）

**役割**: 盤面の優劣を数値化

**評価基準**:
```c
五連:      100,000点  // 勝利確定
開いた四:   50,000点  // 次で勝ち
四連:       10,000点  // 片側ブロック
開いた三:    5,000点  // 攻撃的
三連:        1,000点
開いた二:      500点
二連:          100点
```

**評価方法**:
1. 盤面の全ての石について4方向（横・縦・2斜め）を評価
2. 9マスのラインを抽出してパターンマッチング
3. 相手のパターンは負の評価（防御重視は0.7～0.9倍）
4. 中央付近にボーナス（序盤の安定性）

**主要関数**:
- `evaluate_board()`: 盤面全体の評価
- `evaluate_position()`: 特定位置の評価
- `find_best_move_simple()`: 深さ1の最善手選択

### 3. Alpha-Beta探索（logics/main_engine.c/h）

**役割**: 先読みによる最善手の探索

**アルゴリズム**:
```
function alpha_beta(board, depth, alpha, beta, color, max_color):
    if depth == 0 or game_over:
        return evaluate_board(board, max_color)

    for each move in legal_moves:
        make_move(move)
        score = alpha_beta(board, depth-1, alpha, beta, opponent, max_color)
        undo_move(move)

        if color == max_color:  # Max node
            alpha = max(alpha, score)
            if alpha >= beta: break  # Beta cut
        else:  # Min node
            beta = min(beta, score)
            if alpha >= beta: break  # Alpha cut

    return alpha or beta
```

**パラメータ**:
- デフォルト探索深度: 4
- 最大探索深度: 6（時間があれば）
- Alpha初期値: -∞
- Beta初期値: +∞

**主要関数**:
- `alpha_beta_search()`: 探索のエントリポイント
- `iterative_deepening()`: 反復深化探索（将来拡張用）

### 4. TSS - 詰み探索（logics/TSS.c/h）

**役割**: 即座に勝敗が決する手の高速判定

**機能**:
1. **勝ち手チェック** (`find_winning_move`)
   - 自分が次に打って五連になる手を探す
   - 見つかれば即座に打つ（探索不要）

2. **必死チェック** (`find_threat_move`)
   - 相手が次に打って五連になる手を探す
   - 見つかれば防御手を打つ

**実装の工夫**:
- 全空きマスを走査（盤面サイズ15x15で高速）
- 禁じ手は事前に除外
- 五連判定は4方向の連続カウント

## AI思考フロー

```
calcPutPos() が呼ばれる
    ↓
【初手判定】
    初手? → 中央(7,7)に配置
    ↓
【フェーズ2: TSS】
    自分の勝ち手がある? → その手を打つ
    相手の勝ち手がある? → それを防ぐ
    ↓
【フェーズ3: Alpha-Beta探索】
    深さ4でAlpha-Beta探索
    ↓
最善手を返す
```

## ビルド方法

### 手動ビルド（Windows/MinGW）

```bash
# 各モジュールをコンパイル
gcc -Wall -O3 -c gomoku_ai.c -o gomoku_ai.o
gcc -Wall -O3 -c logics/evaluation.c -o logics/evaluation.o
gcc -Wall -O3 -c logics/main_engine.c -o logics/main_engine.o
gcc -Wall -O3 -c logics/TSS.c -o logics/TSS.o
gcc -Wall -O3 -c gomoku.c -o gomoku.o

# リンク
gcc gomoku.o gomoku_ai.o logics/evaluation.o logics/main_engine.o logics/TSS.o -o gomoku.exe
```

### Makefile使用（推奨）

```bash
make clean
make
```

## 実行方法

```bash
./gomoku.exe
```

## 性能特性

### 探索速度
- 深さ1: 瞬時（<10ms）
- 深さ2: ~50ms
- 深さ3: ~200ms
- 深さ4: ~1秒
- 深さ5: ~5秒
- 深さ6: ~30秒

### メモリ使用量
- ビットボード: ~1KB（盤面表現）
- 探索スタック: ~100KB（深さ6の場合）
- 合計: ~200KB程度

### 棋力推定
- 深さ1: 初級
- 深さ2-3: 中級
- 深さ4-5: 上級
- 深さ6+: 大会レベル（要置換表）

## 今後の拡張

### ステップ4: 高速化技術
- **Zobrist Hashing**: 局面のハッシュ値計算
- **置換表**: 探索済み局面のキャッシュ
- **反復深化**: 時間制限内で深さを増やす
- **手の順序付け**: 良い手を先に探索

### ステップ5: 序盤定石
- オープニングブック（序盤の定跡データベース）
- ファイルからの読み込み
- ハッシュ値による高速検索

### その他の改善
- **評価関数のチューニング**: パターンスコアの調整
- **VCF/VCT探索**: より深い詰み探索
- **学習機能**: 対戦結果から自動チューニング

## デバッグ機能

### ビットマップ表示

`gomoku_ai.c`の先頭で以下を有効化:
```c
#define DEBUG_BITMAPS 1
```

各手番で盤面のビットマップが表示されます。

### ログ出力

各フェーズで以下のような情報が表示されます:
```
[AI] First move at center (7, 7)
[TSS] Winning move found at (8, 8)!
[Alpha-Beta] Searching 45 moves at depth 4...
[Alpha-Beta] Best move: (9, 7) with score 5234
```

## ライセンス

MIT License（プロジェクトルートのLICENSEファイル参照）

## 作成日

2025年10月11日
