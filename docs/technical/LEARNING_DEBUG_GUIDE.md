# 学習システムのデバッグガイド

## 🔍 デバッグ出力の追加

学習システムが正しくファイルを書き込んでいるかを確認するため、詳細なデバッグ出力を追加しました。

## 📝 確認すべきポイント

### 1. ゲーム開始時
プログラム起動時に以下のメッセージが表示されるはずです：
```
[Learning] Learning system initialized.
[AI] First game ever - starting game record
[Learning] Game recording started (move_count reset to 0).
```

### 2. 手の記録
手を打つたびに（最初の10手と、その後10手ごとに）：
```
[Learning] Recorded move #1: (7,7) color=1
[Learning] Recorded move #2: (8,8) color=2
...
```

### 3. ゲーム終了時
新しいゲームが始まると、前のゲームの勝敗が判定されます：
```
[AI] New game detected (move_count=0, previous=20)
[AI] Previous game winner: 1 (AI color: 2)
[AI] Calling finish_game_record with result=-1
[Learning] finish_game_record called with result=-1
========================================
[Learning] Game finished! Result: LOSS
[Learning] Mode: PRACTICE
[Learning] Move count in this game: 20
[Learning] Practice mode: Learning will be performed
[Learning] Total games: 1 (W:0 L:1 D:0)
[Learning] Win rate: 0.00%
```

### 4. 学習データの保存
ゲーム終了時に：
```
[Learning] Attempting to save learning data...
[Learning]   Stats: total=1, wins=0, losses=1, draws=0
[Learning] File opened successfully
[Learning] Wrote stats: 1 records (32 bytes)
[Learning] Wrote pattern data: 1 records (128 bytes)
[Learning] Learning data saved successfully (file closed).
```

### 5. 開局定石の保存（勝利時のみ）
AIが勝利した場合：
```
[Learning] learn_opening_moves called: result=1, move_count=25
[Learning] Opening file opening_book.csv for writing...
[Learning] File opened successfully
[Learning] Writing 5 opening moves...
[Learning]   Move 1: (7,7) - wrote 42 bytes
[Learning]   Move 2: (8,8) - wrote 42 bytes
...
[Learning] Added winning opening moves to book (file closed).
```

## 🐛 問題の診断

### ファイルが作成されない場合

1. **学習モードの確認**
   `config.txt`を確認してください：
   ```
   LEARNING_MODE=PRACTICE
   ```
   `PRODUCTION`になっている場合、ファイルは書き込まれません。

2. **ゲーム終了の検出**
   新しいゲームを開始しないと、前のゲームの勝敗判定が行われません。
   少なくとも**2ゲーム**プレイする必要があります。

3. **書き込み権限**
   プログラムの実行ディレクトリに書き込み権限があることを確認してください。

4. **エラーメッセージの確認**
   以下のようなエラーメッセージが表示されていないか確認：
   ```
   [Learning] ERROR: Could not open learning_data.dat for writing!
   [Learning] Error details: Permission denied
   ```

## 📁 作成されるファイル

### learning_data.dat
- **場所**: プログラムの実行ディレクトリ
- **形式**: バイナリファイル
- **サイズ**: 約160バイト
- **作成タイミング**: ゲーム終了時（練習モードのみ）

### opening_book.csv
- **場所**: プログラムの実行ディレクトリ
- **形式**: CSV（テキストファイル）
- **内容例**:
  ```csv
  move_number,x,y,comment
  1,7,7,Learned from win (game 1)
  2,8,8,Learned from win (game 1)
  3,7,8,Learned from win (game 1)
  4,8,7,Learned from win (game 1)
  5,9,9,Learned from win (game 1)
  ```
- **作成タイミング**: AIが勝利したゲーム終了時（練習モードのみ）

## 🧪 テスト手順

1. **config.txtを確認**
   ```
   LEARNING_MODE=PRACTICE
   ```

2. **既存のファイルを削除**（クリーンなテスト用）
   ```
   del learning_data.dat
   del opening_book.csv
   ```

3. **プログラムを実行**
   ```
   gomoku_new.exe
   ```

4. **1ゲーム目をプレイ**
   - 適当に手を打って、ゲームを終了させる

5. **2ゲーム目を開始**
   - 新しいゲームを開始すると、1ゲーム目の結果が保存される
   - この時点で`learning_data.dat`が作成されるはず

6. **AIに勝たせる**（opening_book.csv作成用）
   - わざと負けて、AIに勝たせる
   - 次のゲーム開始時に`opening_book.csv`が作成される

7. **ファイルの確認**
   ```
   dir learning_data.dat
   dir opening_book.csv
   type opening_book.csv
   ```

## 📊 デバッグ出力の読み方

### 正常なフロー
```
[AI] Initialization complete.
↓
[Learning] Game recording started (move_count reset to 0).
↓
[Learning] Recorded move #1: ...
[Learning] Recorded move #2: ...
...
↓
[AI] New game detected (move_count=0, previous=20)
[AI] Previous game winner: 1 (AI color: 2)
[AI] Calling finish_game_record with result=-1
↓
[Learning] finish_game_record called with result=-1
[Learning] Game finished! Result: LOSS
[Learning] Practice mode: Learning will be performed
↓
[Learning] Attempting to save learning data...
[Learning] File opened successfully
[Learning] Wrote stats: 1 records (32 bytes)
[Learning] Learning data saved successfully (file closed).
```

### 問題があるパターン

**1. 学習が初期化されていない**
```
[Learning] WARNING: record_move called but learning not initialized!
```
→ `init_learning()`が呼ばれていない

**2. ゲーム終了が検出されない**
```
[AI] Calling finish_game_record with result=...
```
このメッセージが表示されない
→ 新しいゲームを開始していない

**3. ファイルが開けない**
```
[Learning] ERROR: Could not open learning_data.dat for writing!
[Learning] Error details: Permission denied
```
→ 書き込み権限がない、またはディレクトリが存在しない

**4. 本番モードになっている**
```
[Learning] Production mode: No learning performed.
```
→ `config.txt`で`LEARNING_MODE=PRACTICE`に変更

## 🔧 トラブルシューティング

### Q: デバッグメッセージが全く表示されない
**A:** `config.txt`の`DEBUG_LEVEL`を確認してください。0の場合、一部のメッセージが表示されません。

### Q: "Game recording started"は表示されるが、"Recorded move"が表示されない
**A:** `record_move()`が呼ばれていません。gomoku_ai.cの`detect_and_record_new_moves()`と、AI思考後の`record_move()`呼び出しを確認してください。

### Q: "finish_game_record"が呼ばれない
**A:** 2ゲーム目を開始していますか？勝敗判定は次のゲーム開始時に行われます。

### Q: ファイルは作成されるが、中身が空
**A:** `fflush()`と`fclose()`の呼び出しを確認してください（既に追加済み）。

---

**最終更新**: 2025年10月14日
**バージョン**: v3.4 デバッグ版
