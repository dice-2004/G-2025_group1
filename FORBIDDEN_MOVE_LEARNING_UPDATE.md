# 禁じ手による勝敗の学習対応レポート

## 実装日
2025年10月16日

## 目的
禁じ手（長連・三三・四四）で勝敗が決まった場合も、通常の勝敗と同様に学習データとして記録し、AIの成長に活用する。

## 実装内容

### 修正ファイル
- `.libgomoku.c`

### 修正箇所

#### 1. 長連の禁じ手検出時の学習処理追加
```c
if((check & CHOLEN) && board[i][j] == STONE_BLACK &&
   (isRoku(board, j, i) == true)) {
    printf("長連の禁じ手です。%sの勝ちです\n", "○");

    // 学習データを保存（黒が禁じ手 → 白の勝ち）
    int ai_color = get_ai_color();
    if (ai_color != 0) {
        int result = (ai_color == STONE_WHITE) ? 1 : -1;
        printf("[GameEnd-Forbidden] Recording result: %d (AI=%d, Winner=WHITE by forbidden move)\n", result, ai_color);
        finish_game_record(result);
        start_game_record();
    }

    return 1;
}
```

#### 2. 三三の禁じ手検出時の学習処理追加（3箇所）
各三三判定箇所に以下の処理を追加:
```c
// 学習データを保存（黒が禁じ手 → 白の勝ち）
int ai_color = get_ai_color();
if (ai_color != 0) {
    int result = (ai_color == STONE_WHITE) ? 1 : -1;
    printf("[GameEnd-Forbidden] Recording result: %d (AI=%d, Winner=WHITE by 3-3)\n", result, ai_color);
    finish_game_record(result);
    start_game_record();
}
```

#### 3. 四四の禁じ手検出時の学習処理追加（3箇所）
各四四判定箇所に以下の処理を追加:
```c
// 学習データを保存（黒が禁じ手 → 白の勝ち）
int ai_color = get_ai_color();
if (ai_color != 0) {
    int result = (ai_color == STONE_WHITE) ? 1 : -1;
    printf("[GameEnd-Forbidden] Recording result: %d (AI=%d, Winner=WHITE by 4-4)\n", result, ai_color);
    finish_game_record(result);
    start_game_record();
}
```

## 学習データの記録方式

### 勝敗結果の決定
- **黒が禁じ手を打った場合**: 白の勝利
  - AIが白番の場合: `result = 1` (勝利)
  - AIが黒番の場合: `result = -1` (敗北)

### 学習データの種類
1. **パターン学習**: 禁じ手に至った局面のパターン評価値を調整
2. **定石学習**: 禁じ手を回避する序盤戦略を学習
3. **早期カットオフ学習**: 禁じ手リスクのある枝の評価調整

## 効果

### AIが黒番の場合
- 禁じ手で負けた場合、その局面のパターンがペナルティを受ける
- 禁じ手を回避する着手を優先的に学習
- 三三・四四を避ける序盤戦略の強化

### AIが白番の場合
- 相手が禁じ手を打った場合、勝利パターンとして学習
- 相手を禁じ手に誘導する戦略の強化
- 黒の禁じ手リスクを利用した攻撃パターンの学習

## デバッグ出力
禁じ手による勝敗時には以下の形式でログを出力:
```
[GameEnd-Forbidden] Recording result: 1 (AI=2, Winner=WHITE by 3-3)
```
- `result`: AIから見た結果 (1=勝利, -1=敗北)
- `AI`: AIの色 (1=BLACK, 2=WHITE)
- `Winner`: 勝者と禁じ手の種類

## 既存機能との統合
- 通常の五並び勝利と同じ学習フローを使用
- `finish_game_record(result)`: 学習データ記録
- `start_game_record()`: 新規ゲーム開始
- LEARNING_MODE_PRACTICE で有効
- LEARNING_MODE_PRODUCTION では統計のみ記録

## テスト方法
1. 黒番AIで意図的に三三を作る局面をプレイ
2. `[GameEnd-Forbidden]` のログを確認
3. `learning_data.dat` に記録されることを確認
4. 次回のゲームで同じパターンが回避されるか確認

## 注意事項
- 禁じ手判定は黒石のみ（白石は禁じ手なし）
- AIが白番の場合、相手の禁じ手による勝利も学習対象
- 学習モードが PRACTICE の場合のみ学習データを更新
- 本番モード (PRODUCTION) では統計情報のみ記録

## ビルド状態
✓ ビルド成功
✓ 全ファイルコンパイル完了
✓ 実行ファイル生成: gomoku_new.exe
