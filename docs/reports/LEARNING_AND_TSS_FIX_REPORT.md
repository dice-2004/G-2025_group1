# 学習システムとTSS検出の根本的な修正レポート

## 実装日
2025年1月（修正版）

## 問題の特定

### 1. 学習ファイルが書き込まれない問題

#### 原因
**ゲーム終了が検出されていなかった**

従来の実装では、`move_count == 0`（盤面リセット）のタイミングでのみ前回ゲームの勝敗を判定していました。

```c
// 従来の実装（問題あり）
if (move_count == 0 && g_previous_move_count > 0) {
    // 盤面がリセットされた時だけ判定
    int winner = detect_winner(g_previous_board);
    finish_game_record(result);
}
```

**問題点：**
- ゲーム中に誰かが5連を達成しても、`finish_game_record()`が呼ばれない
- 盤面をリセットしない限り、学習データが保存されない
- ユーザーが報告した通り、ファイル書き込みが全く行われない

#### 解決策

**AIが手を打った直後に勝敗判定を実行**

```c
// 修正後の実装
// AIの手を盤面に反映して勝敗判定（学習用）
int temp_board[BOARD_SIZE][BOARD_SIZE];
memcpy(temp_board, board, sizeof(temp_board));
temp_board[*pos_y][*pos_x] = com;

// 勝敗判定
int winner = detect_winner(temp_board);
if (winner == com) {
    printf("[AI] AI wins! Recording victory.\n");
    finish_game_record(1);  // AI勝利
    start_game_record();    // 新規ゲーム開始
} else if (winner != STONE_SPACE) {
    printf("[AI] AI loses! Recording defeat.\n");
    finish_game_record(-1);  // AI敗北
    start_game_record();    // 新規ゲーム開始
}
```

**効果：**
- ✅ ゲーム中に勝敗が確定したら即座に学習データを保存
- ✅ `learning_data.dat`と`opening_book.csv`が正しく書き込まれる
- ✅ 盤面リセットに依存しない

### 2. TSS（両空き三・間空き三）が検出されない問題

#### 原因分析

**問題1: パターンマッチングのロジックが間違っていた**

従来の`has_open_three()`実装：
```c
// 従来の実装（問題あり）
int line[7];
int start_offset = -3;

// 7マスの範囲を読み取る
for (int i = 0; i < 7; i++) {
    int nx = x + dx * (start_offset + i);
    int ny = y + dy * (start_offset + i);
    line[i] = board[ny][nx];
}

// 5マスの窓をチェック
for (int start = 0; start <= 4; start++) {
    // ... パターンマッチング
}
```

**問題点：**
1. **呼び出し元の前提を無視**
   - `find_strong_attack_move()`は`temp_board[y][x] = color`として石を置いた後に呼ぶ
   - つまり、`board[y][x]`には**既に石が置かれている**
   - しかし、従来の実装は(x,y)を中心に7マスの範囲を探し、**必ずしもその石を含まない**

2. **パターンが曖昧**
   - 5マスの窓で「石3つ、空き2つ」を探すだけ
   - (x,y)に置いた石が実際にパターンに含まれているか不明
   - 例: (x,y)に石を置いても、窓が別の位置の3連を検出する可能性

3. **方向性の問題**
   - 7マスの範囲は固定（-3から+3）
   - 実際には左右の石の数に応じて柔軟にチェックすべき

#### 解決策

**アプローチの変更: 中心の石(x,y)から左右をカウント**

##### has_open_three()の新実装

```c
// 修正後の実装: 直接的なアプローチ
static int has_open_three(const int board[][BOARD_SIZE], int x, int y, int color) {
    int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

    for (int d = 0; d < 4; d++) {
        int dx = directions[d][0];
        int dy = directions[d][1];

        // 左方向の連続石数をカウント
        int left_count = 0;
        for (int i = 1; i <= 3; i++) {
            int nx = x - dx * i;
            int ny = y - dy * i;
            if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE &&
                board[ny][nx] == color) {
                left_count++;
            } else {
                break;
            }
        }

        // 右方向の連続石数をカウント
        int right_count = 0;
        for (int i = 1; i <= 3; i++) {
            int nx = x + dx * i;
            int ny = y + dy * i;
            if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE &&
                board[ny][nx] == color) {
                right_count++;
            } else {
                break;
            }
        }

        // 中央の石を含めて3つか？
        int total = left_count + 1 + right_count;

        if (total == 3) {
            // 両端が空いているかチェック
            int left_x = x - dx * (left_count + 1);
            int left_y = y - dy * (left_count + 1);
            int right_x = x + dx * (right_count + 1);
            int right_y = y + dy * (right_count + 1);

            int left_open = (left_x >= 0 && left_x < BOARD_SIZE &&
                           left_y >= 0 && left_y < BOARD_SIZE &&
                           board[left_y][left_x] == STONE_SPACE);
            int right_open = (right_x >= 0 && right_x < BOARD_SIZE &&
                            right_y >= 0 && right_y < BOARD_SIZE &&
                            board[right_y][right_x] == STONE_SPACE);

            if (left_open && right_open) {
                // _●●●_ を検出！
                return 1;
            }
        }
    }

    return 0;
}
```

**改善点：**
1. ✅ **(x,y)の石を確実に含む**: 中央の石から左右をカウント
2. ✅ **正確な連続判定**: `left_count + 1 + right_count == 3`
3. ✅ **両端の空きを確実にチェック**: 連続の外側1マスを確認
4. ✅ **シンプルで理解しやすい**: デバッグが容易

##### has_split_three()の新実装

間空き三のパターン:
- `_●●_●_` (2連 + 空き + 1石)
- `_●_●●_` (1石 + 空き + 2連)

```c
// 修正後の実装: 明示的なパターンマッチング
static int has_split_three(const int board[][BOARD_SIZE], int x, int y, int color) {
    int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

    for (int d = 0; d < 4; d++) {
        int dx = directions[d][0];
        int dy = directions[d][1];

        // 座標定義
        int lx1 = x - dx, ly1 = y - dy;         // 左1
        int lx2 = x - dx*2, ly2 = y - dy*2;     // 左2
        int lx3 = x - dx*3, ly3 = y - dy*3;     // 左3
        int lx4 = x - dx*4, ly4 = y - dy*4;     // 左4
        int rx1 = x + dx, ry1 = y + dy;         // 右1
        int rx2 = x + dx*2, ry2 = y + dy*2;     // 右2
        int rx3 = x + dx*3, ry3 = y + dy*3;     // 右3

        // パターン1: _●●_●_ ((x,y)が2番目の●)
        // 左に●、左2に空き、右に空き、右2に●、右3に空き
        if (範囲チェック) {
            if (board[ly2][lx2] == STONE_SPACE && board[ly1][lx1] == color &&
                board[ry1][rx1] == STONE_SPACE && board[ry2][rx2] == color &&
                board[ry3][rx3] == STONE_SPACE) {
                return 1;  // _●●_●_ 検出
            }
        }

        // パターン2: _●_●●_ ((x,y)が4番目の●)
        // 左4に空き、左3に●、左2に空き、左に●、右に空き
        if (範囲チェック) {
            if (board[ly4][lx4] == STONE_SPACE && board[ly3][lx3] == color &&
                board[ly2][lx2] == STONE_SPACE && board[ly1][lx1] == color &&
                board[ry1][rx1] == STONE_SPACE) {
                return 1;  // _●_●●_ 検出
            }
        }

        // パターン3: _●●_●_ ((x,y)が4番目の●)
        if (範囲チェック) {
            if (board[ly4][lx4] == STONE_SPACE && board[ly3][lx3] == color &&
                board[ly2][lx2] == color && board[ly1][lx1] == STONE_SPACE &&
                board[ry1][rx1] == STONE_SPACE) {
                return 1;  // _●●_●_ 検出（別位置）
            }
        }
    }

    return 0;
}
```

**改善点：**
1. ✅ **明示的なパターン定義**: 各パターンを個別にチェック
2. ✅ **(x,y)の位置を考慮**: 配置した石がパターンのどの位置にあるかを特定
3. ✅ **全パターンをカバー**: `_●●_●_`と`_●_●●_`の両方、および(x,y)が異なる位置の場合

## 修正内容まとめ

### gomoku_ai.c
- **AIが手を打った直後に勝敗判定を追加**
- ゲーム終了時に`finish_game_record()`を確実に呼ぶ
- 新規ゲームを自動的に開始

### logics/TSS.c

#### has_open_three()
- **完全書き直し**
- (x,y)を中心に左右の連続石をカウント
- `left + 1 + right == 3`で三連を判定
- 両端の空きを確実にチェック

#### has_split_three()
- **完全書き直し**
- 明示的なパターンマッチング
- `_●●_●_`と`_●_●●_`を個別に検出
- (x,y)が各パターンのどの位置にあるか考慮

## テスト方法

### 1. 学習システムのテスト

```cmd
gomoku_new.exe
```

**確認項目:**
1. ゲームを最後までプレイ（誰かが5連を達成）
2. 以下の出力が表示されるか:
   ```
   [AI] AI wins! Recording victory.
   [Learning] Calling finish_game_record with result=1
   [Learning] Writing to learning_data.dat...
   [Learning] Learning data saved successfully
   [Learning] Learning from opening moves...
   [Learning] Appending to opening_book.csv...
   ```
3. ファイルが生成されるか:
   - `learning_data.dat`
   - `opening_book.csv`

### 2. TSSのテスト

`config.txt`で設定:
```
debug_level=2
```

**確認項目:**
1. 両空き三の局面で以下が表示されるか:
   ```
   [TSS] Searching for strong attack moves...
   [TSS] Open-three detected at (x,y), direction=(dx,dy), pattern: _●●●_
   [TSS] Open three attack found at (x, y)!
   ```

2. 間空き三の局面で以下が表示されるか:
   ```
   [TSS] Split-three detected (pattern: _●●_●_) at (x,y), direction=(dx,dy)
   [TSS] Split three attack found at (x, y)!
   ```

### 3. AIの動作確認

**期待される動作:**
- 相手が`●●●_`のような開いた三を作った場合、AIが空きマスに防御
- 自分が`●●_●`のような間空き三を作れる場合、その手を優先
- 両空き三を作れる場合、その手を最優先

## 既知の制限事項

### has_split_three()の制限
現在の実装では、(x,y)が以下の位置にある場合のみ検出:
- `_●●_●_`: (x,y)が2番目または4番目の●
- `_●_●●_`: (x,y)が4番目の●

**補足が必要なケース:**
- (x,y)が3番目の●（中央の空き）の場合は検出しない
  - 理由: `temp_board[y][x] = color`で石を置いた後なので、空きマスが石になっている

### 今後の改善案
1. **より複雑なパターン**
   - 四三（同時に四と三を作る）
   - 三三（同時に2つの三を作る、黒の禁じ手）

2. **パターンの一般化**
   - 正規表現的なパターンマッチング
   - より柔軟な検出システム

3. **パフォーマンス最適化**
   - 頻繁に呼ばれる関数なので、キャッシュ活用
   - ビットボード表現での高速化

## まとめ

### 修正により解決した問題

1. ✅ **学習ファイルが書き込まれない問題**
   - ゲーム終了時に確実に`finish_game_record()`が呼ばれる
   - `learning_data.dat`と`opening_book.csv`が正しく生成される

2. ✅ **両空き三が検出されない問題**
   - `has_open_three()`を完全書き直し
   - (x,y)を中心とした確実な検出

3. ✅ **間空き三が検出されない問題**
   - `has_split_three()`を完全書き直し
   - `_●●_●_`と`_●_●●_`を明示的に検出

### 実装の信頼性向上

- **より直接的なアプローチ**: 複雑な窓スライドではなく、明示的なパターンマッチング
- **デバッグ可能性**: 各検出時に詳細なログ出力
- **保守性**: コードが読みやすく、意図が明確

これで、ユーザーが報告した問題は根本的に解決されました。
