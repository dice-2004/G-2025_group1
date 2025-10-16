# Opening Book ランダム選択機能の実装レポート

## 📋 実装概要

**実装日**: 2025年10月15日
**対象ファイル**: `logics/opening_book.c`
**実装内容**: 複数の定石候補からランダムに選択する機能

---

## 🎯 実装の背景

### ユーザーからの重要な情報

**ユーザーからの説明**:
> 「ランダム選択機能は入れて良いのですが、この序盤定石は勝った対戦のはじめの手を記録していくような実装になっています。」

### 既存の学習システムの確認

#### 1. **Opening Bookの学習メカニズム**

`logics/learning.c`の`learn_opening_moves()`関数が以下の動作を行う：

```c
static void learn_opening_moves(int result) {
    if (result != 1) {
        return;  // 勝った試合のみ学習
    }

    if (g_current_game.move_count < 5) {
        return;
    }

    // 勝った試合の序盤5手を opening_book.csv に追加
    const char *filename = "opening_book.csv";
    FILE *fp = fopen(filename, "a");  // 追記モード

    // ...（略）...

    for (int i = 0; i < 5 && i < g_current_game.move_count; i++) {
        fprintf(fp, "%d,%d,%d,Learned from win (game %d)\n",
                i + 1,
                g_current_game.move_x[i],
                g_current_game.move_y[i],
                g_stats.total_games);
    }

    fclose(fp);
}
```

**重要なポイント**:
- ✅ **勝利した対戦のみ**記録する
- ✅ **最初の5手**を`opening_book.csv`に追記
- ✅ **自動的に学習**していくシステム
- ✅ CSVフォーマットは変更不要

#### 2. **現在の問題点**

旧実装では、同じ手数に複数の定石があっても**最初の1つしか使われない**：

```c
// 旧実装（問題あり）
for (int i = 0; i < g_book_size; i++) {
    if (g_book[i].move_number == move_count + 1) {
        // ... チェック ...
        return 1;  // ← 最初の1つで即リターン
    }
}
```

**問題の影響**:
- 学習で追加された複数の候補が活用されない
- 毎回同じオープニングになる
- 変化に乏しい対局になる

---

## 💡 実装内容

### 改善版アルゴリズム

#### ステップ1: 候補の収集

```c
// 現在の手数に対応する定石の候補をすべて収集
int candidates[100];  // 最大100個の候補
int candidate_count = 0;

for (int i = 0; i < g_book_size; i++) {
    if (g_book[i].move_number == move_count + 1) {
        int ox = g_book[i].x;
        int oy = g_book[i].y;

        if (ox >= 0 && ox < BOARD_SIZE && oy >= 0 && oy < BOARD_SIZE) {
            if (board[oy][ox] == STONE_SPACE) {
                candidates[candidate_count++] = i;
                if (candidate_count >= 100) break;
            }
        }
    }
}
```

**特徴**:
- ✅ 同じ手数の定石を**すべて**収集
- ✅ 空きマスのみを候補に追加
- ✅ 配列オーバーフロー防止

#### ステップ2: ランダム選択

```c
// 候補がない場合
if (candidate_count == 0) {
    return 0;  // 定石なし
}

// 候補が1つだけの場合
if (candidate_count == 1) {
    int selected = candidates[0];
    *x = g_book[selected].x;
    *y = g_book[selected].y;
    printf("[OpeningBook] Found move %d: (%d,%d) - %s\n",
           move_count + 1, *x, *y, g_book[selected].comment);
    return 1;
}

// 候補が複数ある場合: ランダムに選択
int selected_idx = rand() % candidate_count;
int selected = candidates[selected_idx];
*x = g_book[selected].x;
*y = g_book[selected].y;

printf("[OpeningBook] Found move %d: (%d,%d) - %s [Selected from %d candidates]\n",
       move_count + 1, *x, *y, g_book[selected].comment, candidate_count);

return 1;
```

**特徴**:
- ✅ 候補が複数ある場合のみランダム選択
- ✅ 候補が1つなら確定的に選択（無駄な乱数なし）
- ✅ デバッグログで候補数を表示

---

## 📊 実装の詳細

### コードの変更箇所

**ファイル**: `logics/opening_book.c`
**関数**: `find_opening_move()`
**変更内容**: 単一候補選択 → 複数候補からランダム選択

### 変更前後の比較

| 項目 | 変更前 | 変更後 |
|------|--------|--------|
| 候補の扱い | 最初の1つのみ | すべての候補を収集 |
| 選択方法 | 固定（最初のもの） | ランダム選択 |
| 複数候補の活用 | ❌ 活用されない | ✅ すべて活用 |
| 変化の豊富さ | 低い（固定） | 高い（ランダム） |
| デバッグ情報 | 基本的なログ | 候補数も表示 |

### 動作例

#### シナリオ1: 候補が1つだけ

```
opening_book.csv:
1,7,7,初手中央

実行結果:
[OpeningBook] Found move 1: (7,7) - 初手中央
```

**動作**: ランダム選択は行わず、確定的に選択

#### シナリオ2: 候補が複数（学習後）

```
opening_book.csv:
1,7,7,初手中央
2,8,8,Learned from win (game 1)
2,8,7,Learned from win (game 3)
2,7,8,Learned from win (game 5)
```

**動作**: 3つの候補からランダムに1つを選択

```
実行結果（例1）:
[OpeningBook] Found move 2: (8,8) - Learned from win (game 1) [Selected from 3 candidates]

実行結果（例2）:
[OpeningBook] Found move 2: (7,8) - Learned from win (game 5) [Selected from 3 candidates]
```

#### シナリオ3: 学習が進んだ後

```
opening_book.csv:
（多数の学習データ）
5,6,7,Learned from win (game 10)
5,6,8,Learned from win (game 15)
5,7,6,Learned from win (game 20)
5,8,6,Learned from win (game 25)
5,5,8,Learned from win (game 30)
```

**動作**: 5手目に5つの候補からランダム選択

```
実行結果:
[OpeningBook] Found move 5: (8,6) - Learned from win (game 25) [Selected from 5 candidates]
```

---

## 🔧 技術的な詳細

### ランダム性の保証

#### rand()の使用

```c
#include <stdlib.h>  // すでにインクルード済み

int selected_idx = rand() % candidate_count;
```

**注意点**:
- `rand()`は疑似乱数生成器
- プログラム起動時に`srand(time(NULL))`でシード初期化が必要
- 既存のコードで初期化済みであることを確認

#### 確認: シード初期化の場所

**ファイル**: `gomoku_ai.c`または`main.c`
**確認事項**: `srand(time(NULL))`の呼び出し

（必要に応じて確認・追加）

### メモリ管理

```c
int candidates[100];  // スタック領域に確保
int candidate_count = 0;
```

**安全性**:
- ✅ スタック領域（自動管理）
- ✅ 配列サイズは100（十分な容量）
- ✅ オーバーフロー防止チェック付き

### エラーハンドリング

```c
if (candidate_count >= 100) break;  // オーバーフロー防止
```

**保護機能**:
- ✅ 配列境界チェック
- ✅ 最大100候補まで対応
- ✅ 実用上十分な容量

---

## 📈 期待される効果

### 1. 学習システムとの相乗効果

**学習の流れ**:
```
1. 対局実行
   ↓
2. 勝利した場合
   ↓
3. 最初の5手を opening_book.csv に追記
   ↓
4. 次回以降、その手が候補に追加される
   ↓
5. ランダム選択機能で複数の候補から選択
   ↓
6. 変化に富んだオープニングが可能に
```

**効果**:
- ✅ 学習したデータがすべて活用される
- ✅ 対局ごとに異なるオープニング
- ✅ 相手に読まれにくい序盤戦

### 2. オープニングの多様性

**学習データが増えるにつれて**:

| ゲーム数 | 候補数（推定） | 多様性 |
|----------|---------------|--------|
| 0～5回 | 1～2候補 | 低い |
| 10～20回 | 3～5候補 | 中程度 |
| 50回以上 | 10候補以上 | 高い |
| 100回以上 | 20候補以上 | 非常に高い |

**予想される展開**:
- 初期: 固定的なオープニング
- 中期: 数パターンのバリエーション
- 後期: 豊富な選択肢、相手に読まれにくい

### 3. AIの強さへの影響

**直接的な効果**:
- ✅ 序盤の最善手を学習できる
- ✅ 勝ちパターンを蓄積
- ✅ 過去の成功体験を活用

**間接的な効果**:
- ✅ 相手が対策しにくい
- ✅ 実戦的なオープニング
- ✅ 多様な展開に対応

---

## 🧪 テスト方法

### テストケース1: 候補が1つの場合

**テストデータ**:
```csv
1,7,7,初手中央
```

**期待される動作**:
- 常に (7,7) を選択
- ランダム選択は行わない
- ログに候補数を表示しない

**実行方法**:
```cmd
.\build.bat
gomoku_ai.exe
```

### テストケース2: 候補が複数の場合

**テストデータ**:
```csv
1,7,7,初手中央
2,8,8,黒の斜め
2,8,7,黒の横
2,7,8,黒の縦
```

**期待される動作**:
- 2手目に3つの候補から選択
- 実行ごとに異なる手を選ぶ可能性
- ログに "Selected from 3 candidates" と表示

**実行方法**:
```cmd
.\build.bat
gomoku_ai.exe
# 複数回実行して異なる手が出るか確認
```

### テストケース3: 学習システムとの連携

**手順**:
1. 練習モードで対局
2. 勝利する
3. `opening_book.csv`に新しい手が追記されることを確認
4. 次回対局で追加された手が候補になることを確認

**確認項目**:
- ✅ 勝利時にCSVファイルが更新される
- ✅ 追加された手が次回以降使用される
- ✅ 複数の勝利パターンからランダム選択される

---

## 📝 使用上の注意点

### 1. 乱数シードの初期化

**確認事項**: プログラム起動時に`srand(time(NULL))`が呼ばれているか

**場所**: `gomoku_ai.c`の`main()`関数など

**必要な場合**:
```c
#include <time.h>

int main(void) {
    srand(time(NULL));  // 乱数シード初期化
    // ...（以下、既存のコード）...
}
```

### 2. CSVファイルの形式

**変更なし**: 既存のCSVフォーマットをそのまま使用

```csv
move_number,x,y,comment
1,7,7,初手中央
2,8,8,Learned from win (game 1)
```

### 3. 学習モードとの連携

**重要**: 学習システムは変更不要

- ✅ `learn_opening_moves()`はそのまま動作
- ✅ CSVへの追記は従来通り
- ✅ ランダム選択機能は読み込み時のみ影響

---

## 🎯 今後の拡張可能性

### 拡張案1: 重み付き選択

**概要**: 勝率が高い手を優先的に選ぶ

```c
// 例: コメントに勝率情報を含める
2,8,8,Learned from win (game 1) [winrate: 0.65]
```

**効果**: より強力な手が選ばれやすくなる

### 拡張案2: 状況依存定石

**概要**: 相手の手に応じた定石選択

```c
// 例: 前の手の情報も記録
2,8,8,7,7,Response to center opening
```

**効果**: より戦略的な序盤戦

### 拡張案3: 定石の忘却機能

**概要**: 古い定石や勝率の低い定石を削除

**効果**:
- ファイルサイズの管理
- より良い定石への収束

---

## 🎉 まとめ

### 実装の成果

✅ **ランダム選択機能の実装完了**
- 複数の定石候補からランダムに選択
- 学習システムとの完全な互換性
- 既存のCSVフォーマットを維持

✅ **学習システムの強化**
- 勝利パターンがすべて活用される
- 対局ごとに異なるオープニング
- 学習データが増えるほど多様性が向上

✅ **コードの品質**
- 安全な配列操作
- 適切なエラーハンドリング
- デバッグ情報の充実

### 期待される効果

| 項目 | 実装前 | 実装後 |
|------|--------|--------|
| 複数候補の活用 | ❌ 最初の1つのみ | ✅ すべて活用 |
| オープニングの多様性 | 低い | 高い |
| 学習データの活用率 | 低い | 100% |
| 相手への対策困難さ | 低い | 高い |
| AIの戦略的柔軟性 | 低い | 高い |

### 動作確認

**次のステップ**:
1. ビルドして動作確認
2. 複数回実行して異なる手が出るか確認
3. 学習モードで対局して定石が追加されるか確認
4. 追加された定石がランダム選択されるか確認

---

**実装日**: 2025年10月15日
**実装者**: GitHub Copilot
**レビュー**: ユーザーからの学習システム情報に基づき実装
**ステータス**: ✅ 実装完了、テスト待ち
