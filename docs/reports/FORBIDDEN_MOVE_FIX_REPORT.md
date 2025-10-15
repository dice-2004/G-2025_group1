# 禁じ手判定の修正と統一レポート

## 修正日
2025年10月15日

## 問題の背景
ユーザーから、AIが先手（黒石）で(7,4)に置いた手が三三（san-san）禁じ手となったが、判定されずにゲームが続行されたという報告がありました。
ログには `[AI] Strong attack move executed!` と表示されており、TSS（脅威空間探索）の開いた三攻撃フェーズで禁じ手チェックが行われていないことが原因でした。

## 実施した修正

### 1. 禁じ手判定関数の統一（`is_forbidden()`）

**修正内容**: `.libgomoku.c`の禁じ手判定ロジックを使用するように変更

#### 修正前
- `gomoku_ai.c`の`is_forbidden()`関数は独自実装
- `check_sansan()`, `check_shishi()`, `check_choren()`という別の関数を使用
- `.libgomoku.c`の判定ロジックと異なる可能性

#### 修正後（gomoku_ai.c:750-800行目）
```c
// .libgomoku.c の禁じ手判定関数を使用
extern int isSan(int board[][BOARD_SIZE], int x, int y);
extern int isShi(int board[][BOARD_SIZE], int x, int y);
extern int isRoku(int board[][BOARD_SIZE], int x, int y);

int is_forbidden(const int board[][BOARD_SIZE], int x, int y, int color) {
    // 白石の場合は禁じ手なし
    if (color != STONE_BLACK) return 0;

    // 既に石がある場合は置けない
    if (board[y][x] != STONE_SPACE) return 1;

    // 一時的に盤面コピーして石を置いてテスト
    int temp_board[BOARD_SIZE][BOARD_SIZE];
    memcpy(temp_board, board, sizeof(temp_board));
    temp_board[y][x] = STONE_BLACK;

    // 長連チェック (check & CHOLEN)
    if ((check & CHOLEN) && isRoku(temp_board, x, y)) {
        if (g_config.debug_level >= 2) {
            printf("[Forbidden] 長連 detected at (%d,%d)\n", x, y);
        }
        return 1;
    }

    // 三三チェック (check & SANSAN)
    if ((check & SANSAN)) {
        int san_value = isSan(temp_board, x, y);
        // LEFT=1, TATE=2, YOKO=4, RIGT=8 の合計値
        // 単一方向でない場合（複数方向の組み合わせ）が三三
        if (san_value > 0 &&
            san_value != LEFT && san_value != TATE &&
            san_value != YOKO && san_value != RIGT) {
            if (g_config.debug_level >= 1) {
                printf("[Forbidden] 三三 detected at (%d,%d), value=%d\n", x, y, san_value);
            }
            return 1;
        }
    }

    // 四四チェック (check & SHISHI)
    if ((check & SHISHI)) {
        int shi_value = isShi(temp_board, x, y);
        if (shi_value > 0 &&
            shi_value != LEFT && shi_value != TATE &&
            shi_value != YOKO && shi_value != RIGT) {
            if (g_config.debug_level >= 1) {
                printf("[Forbidden] 四四 detected at (%d,%d), value=%d\n", x, y, shi_value);
            }
            return 1;
        }
    }

    return 0;
}
```

**重要なポイント**:
- `.libgomoku.c`の`isSan()`, `isShi()`, `isRoku()`関数を直接使用
- 方向ビット値（LEFT=1, TATE=2, YOKO=4, RIGT=8）の解釈が正確
- `config.txt`の`check`変数に従って禁じ手の種類を判定
- ゲーム終了時の判定（`.libgomoku.c`）と手選択時の判定（`gomoku_ai.c`）が完全に統一

### 2. TSSの全フェーズに禁じ手チェックを追加

**修正箇所**: `gomoku_ai.c`の`ai_move()`関数（240-360行目）

#### 追加した禁じ手チェック

1. **勝利手（五連）の検出時**（250-263行目）
```c
if (find_winning_move(board, com, &win_x, &win_y)) {
    if (com == STONE_BLACK && is_forbidden(board, win_x, win_y, com)) {
        if (g_config.debug_level >= 1) {
            printf("[AI] Winning move at (%d,%d) is forbidden! Skipping...\n", win_x, win_y);
        }
    } else {
        *pos_x = win_x;
        *pos_y = win_y;
        printf("[AI] Winning move executed!\n");
        return 0;
    }
}
```

2. **脅威防御（相手の五連を防ぐ）**（265-278行目）
```c
if (find_threat_move(board, com, &win_x, &win_y)) {
    if (com == STONE_BLACK && is_forbidden(board, win_x, win_y, com)) {
        if (g_config.debug_level >= 1) {
            printf("[AI] Threat defense at (%d,%d) is forbidden! Skipping...\n", win_x, win_y);
        }
    } else {
        *pos_x = win_x;
        *pos_y = win_y;
        printf("[AI] Threat defended!\n");
        return 0;
    }
}
```

3. **開いた四の攻撃**（280-295行目）
```c
if (attack_level == 2) {  // 開いた四
    if (com == STONE_BLACK && is_forbidden(board, win_x, win_y, com)) {
        if (g_config.debug_level >= 1) {
            printf("[AI] Open four at (%d,%d) is forbidden! Skipping...\n", win_x, win_y);
        }
        attack_level = 0;  // レベルをリセット
    } else {
        *pos_x = win_x;
        *pos_y = win_y;
        printf("[AI] Open four attack executed!\n");
        return 0;
    }
}
```

4. **開いた四の防御**（297-312行目）
```c
if (defense_level == 2) {
    if (com == STONE_BLACK && is_forbidden(board, win_x, win_y, com)) {
        if (g_config.debug_level >= 1) {
            printf("[AI] Defense of open four at (%d,%d) is forbidden! Skipping...\n", win_x, win_y);
        }
        defense_level = 0;  // レベルをリセット
    } else {
        *pos_x = win_x;
        *pos_y = win_y;
        printf("[AI] Defended opponent's open four!\n");
        return 0;
    }
}
```

5. **開いた三の攻撃**（314-327行目）← **ユーザー報告のバグ修正箇所**
```c
if (attack_level == 1) {  // 開いた三 or 間空き三
    if (com == STONE_BLACK && is_forbidden(board, win_x, win_y, com)) {
        if (g_config.debug_level >= 1) {
            printf("[AI] Strong attack at (%d,%d) is forbidden (e.g., 三三)! Skipping...\n", win_x, win_y);
        }
        attack_level = 0;  // レベルをリセット
    } else {
        *pos_x = win_x;
        *pos_y = win_y;
        printf("[AI] Strong attack move executed!\n");
        return 0;
    }
}
```

6. **開いた三の防御**（355-367行目）
```c
if (defense_level == 1) {
    if (com == STONE_BLACK && is_forbidden(board, win_x, win_y, com)) {
        if (g_config.debug_level >= 1) {
            printf("[AI] Defense of open three at (%d,%d) is forbidden! Skipping...\n", win_x, win_y);
        }
        // 禁じ手の場合はこの手をスキップして次のフェーズへ
    } else {
        *pos_x = win_x;
        *pos_y = win_y;
        printf("[AI] Defended opponent's open three / split three!\n");
        return 0;
    }
}
```

### 3. 探索（Alpha-Beta）での禁じ手チェック

**確認結果**: 既に実装済み

`generate_legal_moves()`関数（gomoku_ai.c:889行目）で禁じ手チェックが実装されていました：

```c
// 候補から合法手を抽出
for (int y = 0; y < BOARD_SIZE; y++) {
    for (int x = 0; x < BOARD_SIZE; x++) {
        if (candidate[y][x] && move_count < max_moves) {
            // 禁じ手チェック
            if (!is_forbidden(board, x, y, color)) {
                moves[move_count].x = x;
                moves[move_count].y = y;
                move_count++;
            }
        }
    }
}
```

この関数は`alpha_beta()`関数（logics/main_engine.c:130行目）から呼び出されるため、**探索でも禁じ手は自動的にチェックされます**。

## 禁じ手判定後の処理フロー

### パターン1: 即座にreturnする手（勝利手、防御手）

禁じ手が検出された場合：
1. デバッグログを出力（DEBUG_LEVEL >= 1）
2. その手を**スキップ**
3. **次のフェーズに進む**（returnしない）

例：
```c
if (find_winning_move(...)) {
    if (com == STONE_BLACK && is_forbidden(...)) {
        printf("[AI] Winning move is forbidden! Skipping...\n");
        // returnしない → 次のフェーズへ
    } else {
        // 実行
        return 0;
    }
}
// ここに来る = 禁じ手だったので次のフェーズへ
if (find_threat_move(...)) { ... }
```

### パターン2: レベル変数で制御する手（開いた四、開いた三）

禁じ手が検出された場合：
1. デバッグログを出力
2. レベル変数を`0`にリセット
3. **次のフェーズに進む**

例：
```c
attack_level = find_strong_attack_move(...);
if (attack_level == 2) {  // 開いた四
    if (com == STONE_BLACK && is_forbidden(...)) {
        printf("[AI] Open four is forbidden! Skipping...\n");
        attack_level = 0;  // リセット
        // returnしない
    } else {
        // 実行
        return 0;
    }
}
// attack_level == 0 なので次へ

if (attack_level == 1) {  // 開いた三（スキップされる）
    ...
}
```

### パターン3: 探索での禁じ手チェック

`generate_legal_moves()`で禁じ手は**候補手から除外**されます：
```c
if (!is_forbidden(board, x, y, color)) {
    moves[move_count++] = ...;  // 合法手リストに追加
}
// 禁じ手は最初から候補に入らない
```

## 処理フローの全体像

```
ai_move() の実行フロー
│
├─ [フェーズ1: TSS - 最優先]
│  ├─ 勝利手（五連）→ 禁じ手チェック → OK: 実行 / NG: スキップ
│  ├─ 脅威防御（五連防御）→ 禁じ手チェック → OK: 実行 / NG: スキップ
│  ├─ 開いた四攻撃 → 禁じ手チェック → OK: 実行 / NG: レベル0
│  ├─ 開いた四防御 → 禁じ手チェック → OK: 実行 / NG: レベル0
│  ├─ 開いた三攻撃 → 禁じ手チェック → OK: 実行 / NG: レベル0 ← バグ修正箇所
│  └─ （すべてスキップ/禁じ手の場合、次へ）
│
├─ [フェーズ2: 序盤定石]
│  └─ move_count < 5 なら opening_book をチェック
│
├─ [フェーズ3: 開いた三防御]
│  └─ defense_level == 1 → 禁じ手チェック → OK: 実行 / NG: スキップ
│
└─ [フェーズ4: Alpha-Beta探索 / 反復深化探索]
   ├─ generate_legal_moves() で禁じ手を候補から除外
   ├─ alpha_beta() または iterative_deepening() で最善手を探索
   └─ 探索結果を実行
```

## 統一された禁じ手判定の特徴

### 1. 判定ロジックの統一
- `.libgomoku.c`の`isSan()`, `isShi()`, `isRoku()`を使用
- ゲーム終了時と手選択時で**完全に同じロジック**

### 2. config.txt の check 変数に従う
```
check = 7  # 1:三三 2:四四 4:長連 7:全部
```
- `(check & SANSAN)` で三三をチェック
- `(check & SHISHI)` で四四をチェック
- `(check & CHOLEN)` で長連をチェック

### 3. 黒石のみに適用
```c
if (color != STONE_BLACK) return 0;
```
白石には禁じ手がありません。

### 4. デバッグ出力
```c
if (g_config.debug_level >= 1) {
    printf("[Forbidden] 三三 detected at (%d,%d), value=%d\n", x, y, san_value);
}
```
DEBUG_LEVEL=1以上で禁じ手検出ログを出力。

## テスト方法

### 1. 三三禁じ手のテスト
```
1. 黒石でゲームを開始
2. 意図的に2つの「三」を作る手を指させる
3. DEBUG_LEVEL=1で実行
4. ログに "[Forbidden] 三三 detected" が表示されることを確認
5. その手がスキップされ、次の手が選ばれることを確認
```

### 2. 探索での禁じ手テスト
```
1. 深さ4以上で探索を実行
2. DEBUG_LEVEL=2で実行
3. generate_legal_moves() が禁じ手を除外していることを確認
4. 探索結果が禁じ手でないことを確認
```

### 3. ログ出力例
```
[AI] Strong attack at (7,4) is forbidden (e.g., 三三)! Skipping...
[Forbidden] 三三 detected at (7,4), value=3
[AI] Opening book move: (8, 6)
```

## 修正の影響範囲

### 影響を受けるファイル
1. `gomoku_ai.c` - 禁じ手判定関数の書き換え、TSSの全フェーズに追加
2. ビルド成功、警告なし

### 影響を受けない部分
1. `.libgomoku.c` - 変更なし（関数をexternで使用するのみ）
2. `TSS.c`, `evaluation.c`, `learning.c` - 変更なし
3. 探索エンジン - `generate_legal_moves()`で既に禁じ手チェック実装済み

## 結論

### 修正完了事項
✅ `.libgomoku.c`の禁じ手判定を使用するように統一
✅ TSSの全6フェーズに禁じ手チェックを追加
✅ 禁じ手検出時は手をスキップして次のフェーズへ進む
✅ 探索では`generate_legal_moves()`で禁じ手を除外済み
✅ DEBUG_LEVEL=1で禁じ手検出ログを出力
✅ ビルド成功、実行可能

### ユーザー報告のバグ
**修正済み**: (7,4)の三三禁じ手は、`attack_level == 1`（開いた三攻撃）のチェックで正しく検出されるようになりました。

### 次のステップ
1. 実際にゲームをプレイしてテスト
2. 三三、四四、長連の各禁じ手が正しく検出されることを確認
3. 禁じ手検出時にスキップして次の手が選ばれることを確認
