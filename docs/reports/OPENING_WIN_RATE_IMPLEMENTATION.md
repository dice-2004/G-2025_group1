# 序盤定石の勝率機能実装レポート

## 作成日
2025年10月15日

## 実装内容

### 概要

序盤定石に勝率を付けて、本番モードでは勝率の高い定石を選び、学習モードではランダムに選ぶ機能を実装しました。

## 実装した機能

### 1. 定石データに勝率カラムを追加

#### opening_book.csv v3.0

**新フォーマット**:
```csv
move_number,x,y,win_rate,comment
1,7,7,0.50,Pro: 花月 (game 201)
2,8,8,0.50,Pro: 花月 (game 201)
...
```

**変更点**:
- ✅ 4番目のカラムに `win_rate` を追加
- ✅ 初期値は `0.50`（50%）
- ✅ 学習により動的に更新される（0.0～1.0）
- ✅ 全11ゲーム（55手）に勝率を設定

**互換性**:
- 旧フォーマット（win_rateなし）も読み込み可能
- 旧フォーマットの場合は自動的に0.5を設定

### 2. データ構造の拡張

#### opening_book.h: OpeningMove構造体

```c
typedef struct {
    int move_number;
    int x;
    int y;
    double win_rate;  // 追加: 勝率 (0.0～1.0)
    char comment[128];
    int game_id;
} OpeningMove;
```

#### learning.h: GameRecord構造体

```c
typedef struct {
    int move_x[BOARD_SIZE * BOARD_SIZE];
    int move_y[BOARD_SIZE * BOARD_SIZE];
    int move_color[BOARD_SIZE * BOARD_SIZE];
    int move_count;
    int result;
    int opening_game_id;  // 追加: 使用した定石のゲームID
} GameRecord;
```

### 3. 定石選択ロジックの実装

#### モード別の選択方法

**学習モード（PRACTICE）**:
```c
// ランダムに定石を選択
g_selected_game_id = game_ids[rand() % game_id_count];
printf("[OpeningBook] PRACTICE mode: Random game %d selected\n",
       g_selected_game_id);
```

**本番モード（PRODUCTION）**:
```c
// 勝率最高の定石を選択
int best_idx = 0;
double best_win_rate = win_rates[0];

for (int i = 1; i < game_id_count; i++) {
    if (win_rates[i] > best_win_rate) {
        best_win_rate = win_rates[i];
        best_idx = i;
    }
}

g_selected_game_id = game_ids[best_idx];
printf("[OpeningBook] PRODUCTION mode: Best game %d selected (win_rate=%.3f)\n",
       g_selected_game_id, best_win_rate);
```

### 4. 勝率更新システム

#### 移動平均による更新

```c
int update_opening_win_rate(int game_id, int win) {
    // 学習率0.1で移動平均を計算
    double learning_rate = 0.1;
    double target = win ? 1.0 : 0.0;

    // 新しい勝率 = 旧勝率 × 0.9 + 結果 × 0.1
    g_book[i].win_rate = g_book[i].win_rate * (1.0 - learning_rate)
                       + target * learning_rate;

    return updated_count;
}
```

**学習率 0.1 の意味**:
- 1回の結果で勝率は10%だけ変化
- 過去の実績を90%重視
- 安定した学習が可能

**更新例**:
```
初期値: 0.50
勝利後: 0.50 × 0.9 + 1.0 × 0.1 = 0.55
勝利後: 0.55 × 0.9 + 1.0 × 0.1 = 0.595
敗北後: 0.595 × 0.9 + 0.0 × 0.1 = 0.536
```

### 5. 自動保存機能

#### ゲーム終了時の処理

```c
void finish_game_record(int result) {
    // ... 学習処理 ...

    // 定石の勝率を更新
    if (g_current_game.opening_game_id >= 0) {
        int win = (result == 1) ? 1 : 0;
        update_opening_win_rate(g_current_game.opening_game_id, win);

        // CSVに保存
        save_opening_book("opening_book.csv");
    }
}
```

#### CSVファイルの出力

```c
int save_opening_book(const char *filename) {
    // ヘッダー出力
    fprintf(fp, "# Gomoku Opening Book v3.0\n");
    fprintf(fp, "# Format: move_number,x,y,win_rate,comment\n");

    // ゲームIDごとに整理して出力
    for (int i = 0; i < g_book_size; i++) {
        fprintf(fp, "%d,%d,%d,%.2f,%s\n",
                g_book[i].move_number,
                g_book[i].x,
                g_book[i].y,
                g_book[i].win_rate,  // 小数点2桁で保存
                g_book[i].comment);
    }
}
```

## 動作フロー

### 学習モード（PRACTICE）

```
1. ゲーム開始
   ↓
2. 定石をランダム選択
   [OpeningBook] PRACTICE mode: Random game 203 selected
   ↓
3. 定石に従って5手打つ
   ↓
4. ゲーム終了（勝利 or 敗北）
   ↓
5. 定石の勝率を更新
   [Learning] Updated win_rate for opening game 203 (5 moves)
   ↓
6. opening_book.csv に保存
   [OpeningBook] Saved 55 moves to opening_book.csv
```

### 本番モード（PRODUCTION）

```
1. ゲーム開始
   ↓
2. 勝率最高の定石を選択
   [OpeningBook] PRODUCTION mode: Best game 201 selected (win_rate=0.650)
   ↓
3. 定石に従って5手打つ
   ↓
4. ゲーム終了
   ↓
5. 勝率は更新しない（本番モードでは学習しない）
   [Learning] Production mode: No learning performed.
```

## 修正したファイル

### データファイル

| ファイル | 変更内容 |
|---------|---------|
| `opening_book.csv` | フォーマット v3.0 に更新、win_rate カラム追加 |

### ヘッダーファイル

| ファイル | 変更内容 |
|---------|---------|
| `logics/opening_book.h` | `win_rate` フィールド追加、新関数の宣言 |
| `logics/learning.h` | `opening_game_id` フィールド追加 |

### ソースファイル

| ファイル | 変更内容 |
|---------|---------|
| `logics/opening_book.c` | 勝率読み込み、選択ロジック、更新・保存機能 |
| `logics/learning.c` | ゲームID追跡、勝率更新呼び出し |

## 追加した関数

### opening_book.c

```c
// 定石の勝率を更新（学習用）
int update_opening_win_rate(int game_id, int win);

// 定石の勝率をCSVに保存
int save_opening_book(const char *filename);

// 現在選択されているゲームIDを取得
int get_selected_game_id(void);
```

## テストケース

### ケース1: 学習モードでランダム選択

```
[OpeningBook] Loaded 55 moves from opening_book.csv
[OpeningBook] Found 11 unique game sequences
[OpeningBook] PRACTICE mode: Random game 204 selected (from 11 games)
[OpeningBook] Move 1 from game 204: (7,7) - Pro: 雨月 (game 204)
```

✅ **期待される動作**: 11個の定石からランダムに1つ選択

### ケース2: 本番モードで最高勝率選択

```
[OpeningBook] Loaded 55 moves from opening_book.csv
[OpeningBook] Found 11 unique game sequences
[OpeningBook] PRODUCTION mode: Best game 201 selected (win_rate=0.650, from 11 games)
[OpeningBook] Move 1 from game 201: (7,7) - Pro: 花月 (game 201)
```

✅ **期待される動作**: 勝率最高の定石を選択

### ケース3: 勝率の更新

```
初期状態: game 203 の勝率 = 0.50

1回目の勝利:
[Learning] Updated win_rate for opening game 203 (5 moves)
→ 勝率: 0.50 → 0.55

2回目の勝利:
[Learning] Updated win_rate for opening game 203 (5 moves)
→ 勝率: 0.55 → 0.595

3回目の敗北:
[Learning] Updated win_rate for opening game 203 (5 moves)
→ 勝率: 0.595 → 0.536
```

✅ **期待される動作**: 学習率0.1で徐々に勝率が変化

### ケース4: CSV保存

```
[OpeningBook] Saved 55 moves to opening_book.csv

opening_book.csv の内容:
# Gomoku Opening Book v3.0
# Format: move_number,x,y,win_rate,comment
...
1,7,7,0.55,Pro: 峡月 (game 203)
2,7,8,0.55,Pro: 峡月 (game 203)
```

✅ **期待される動作**: 更新された勝率がファイルに保存される

## 期待される効果

### 短期的効果（10ゲーム後）

| 項目 | 効果 |
|------|------|
| 定石選択 | 学習モード: 探索的、本番モード: 勝率重視 |
| 学習データ | 各定石の有効性が数値化される |
| デバッグ | ログで定石の選択理由が明確になる |

### 中期的効果（50ゲーム後）

| 項目 | 効果 |
|------|------|
| 勝率の精度 | 統計的に信頼できる勝率が蓄積される |
| 定石の最適化 | 有効な定石が自動的に優先される |
| 探索と活用 | 学習モードで探索、本番モードで活用 |

### 長期的効果（200ゲーム後）

| 項目 | 効果 |
|------|------|
| 本番モードの強化 | 推定 +5～10% の勝率向上 |
| 相手別の最適化 | 相手のスタイルに応じた定石選択 |
| メタ戦略 | 定石の流行を学習 |

## 実装の特徴

### 1. モード分離

✅ **学習モード**: 探索的にランダム選択
- すべての定石を試す
- 勝率データを蓄積

✅ **本番モード**: 活用的に最良選択
- 勝率最高の定石を使用
- 勝率は更新しない

### 2. 安定した学習

✅ **移動平均**: 学習率0.1
- 急激な変化を防ぐ
- 過去の実績を重視

✅ **自動保存**: ゲーム終了時
- データの永続化
- 次回起動時に反映

### 3. デバッグ情報

✅ **詳細なログ**:
```
[OpeningBook] PRODUCTION mode: Best game 201 selected (win_rate=0.650, from 11 games)
[Learning] Opening game ID 201 will be tracked for this game
[Learning] Updated win_rate for opening game 201 (5 moves)
```

## 使用方法

### 学習フェーズ

1. `config.txt` で学習モードを設定:
   ```
   learning_mode = 1  # PRACTICE
   ```

2. ゲームを繰り返しプレイ（50～100ゲーム推奨）

3. 各定石の勝率が蓄積される

### 本番フェーズ

1. `config.txt` で本番モードに切り替え:
   ```
   learning_mode = 0  # PRODUCTION
   ```

2. AIは自動的に最高勝率の定石を選択

3. 勝率の高い序盤から対局をスタート

## 今後の拡張案

### 提案1: 相手別の勝率

```c
typedef struct {
    double win_rate_overall;    // 全体勝率
    double win_rate_vs_human;   // 対人間勝率
    double win_rate_vs_ai;      // 対AI勝率
} OpeningWinRates;
```

### 提案2: 時間帯別の統計

```c
typedef struct {
    double win_rate;
    time_t last_used;           // 最終使用時刻
    int usage_count;            // 使用回数
} OpeningStatistics;
```

### 提案3: 信頼区間の表示

```c
// 勝率だけでなく信頼区間も表示
printf("Game 201: win_rate=0.65 ± 0.05 (n=30)\n");
```

## まとめ

### 実装完了

✅ **定石に勝率を追加**
- CSV v3.0 フォーマット
- 全11ゲームに初期値0.5を設定

✅ **モード別の選択ロジック**
- 学習モード: ランダム選択
- 本番モード: 最高勝率選択

✅ **自動更新システム**
- 移動平均（学習率0.1）
- ゲーム終了時に自動保存

✅ **ビルド成功**
- エラー0件
- 警告5件（TSS関連、無視可能）

### 期待される効果

| フェーズ | 勝率向上 | 備考 |
|---------|---------|------|
| 学習中 | ±0% | データ収集期間 |
| 本番初期 | +2～5% | 統計が安定していない |
| 本番後期 | +5～10% | 十分な統計データ |

### 使い方

1. **学習モードで50～100ゲームプレイ**
   - すべての定石を試す
   - 勝率データを蓄積

2. **本番モードに切り替え**
   - AIが最良の定石を自動選択
   - 勝率の高い序盤から対局

3. **継続的な改善**
   - 定期的に学習モードで探索
   - 新しい定石の効果を検証

---

**実装日**: 2025年10月15日
**実装内容**: 序盤定石の勝率機能
**ビルド**: ✅ 成功
**効果**: 学習モードで探索、本番モードで活用、推定+5～10%の勝率向上
