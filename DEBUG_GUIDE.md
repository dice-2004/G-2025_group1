# 詳細デバッグ手順

## 🔍 現在の問題

ユーザーからの報告：
1. **TSSが開いた三を正しく検出していない**
2. **学習ファイルが作成されない**

## 🛠️ デバッグ方法

### 1. 詳細なデバッグ出力を有効にする

`config.txt`を編集：
```ini
# 詳細なTSS検出ログを表示
debug_level=3

# その他の設定はそのまま
learning_mode=PRACTICE
iterative_deepening=1
```

### 2. プログラムを実行

```cmd
.\gomoku_new.exe
```

### 3. テストケース

#### テスト1: 開いた三の防御

**手順:**
1. 先手を選ぶ
2. 以下の手を打つ：
   ```
   ●: (7,7)
   ○: (7,8)
   ●: (6,7)
   ○: (8,8)
   ●: (8,7)  ← これで _●●●_ ができる
   ```

**期待される動作:**
```
[TSS] ===== Searching for DEFENSE moves =====
[TSS-DETAIL] Checking open-three at (5,7) color=1
[TSS-DETAIL]   dir(1,0): left=0, right=2, total=3
[TSS-DETAIL]   left_pos(4,7)=1, right_pos(8,7)=1
[TSS] ★★★ Open-three DETECTED at (5,7), dir=horizontal(1,0), L=0 R=2 ★★★
[AI] Defended opponent's open three!
```

AIは`(5,7)`または`(9,7)`に防御すべき。

#### テスト2: 学習ファイルの作成

**手順:**
1. ゲームを最後までプレイ（誰かが5連達成）
2. 以下の出力を確認：

**期待される出力:**
```
[AI] Game already finished! Winner: 1 (AI color: 2)
[AI] Recording game result: -1
[Learning] finish_game_record called with result=-1
[Learning] Current directory: C:\Users\hddai\oithomes\c\kadai\G-2025_group1
[Learning] File opened successfully
[Learning] Learning data saved successfully
```

3. カレントディレクトリでファイルを確認：
```cmd
dir learning_data.dat
dir opening_book.csv
```

## 📊 デバッグ出力の読み方

### TSS検出のログ

```
[TSS-DETAIL] Checking open-three at (5,7) color=1
```
- `(5,7)` = x=5, y=7 の位置をチェック中
- `color=1` = 黒石の開いた三を探している

```
[TSS-DETAIL]   dir(1,0): left=0, right=2, total=3
```
- `dir(1,0)` = 横方向（→）
- `left=0` = 左側に0個の連続石
- `right=2` = 右側に2個の連続石
- `total=3` = 中央含めて3個（開いた三の条件）

```
[TSS-DETAIL]   left_pos(4,7)=1, right_pos(8,7)=1
```
- `left_pos(4,7)=1` = 左端(4,7)が空き（1=true）
- `right_pos(8,7)=1` = 右端(8,7)が空き（1=true）
- 両端が空き → **開いた三を検出**

```
[TSS] ★★★ Open-three DETECTED at (5,7), dir=horizontal(1,0), L=0 R=2 ★★★
```
- `(5,7)`に置くと横方向の開いた三ができる
- `L=0 R=2` = パターンは `●●●__` の形

### 方向の意味

- `horizontal (1,0)` = 横 →
- `vertical (0,1)` = 縦 ↓
- `diagonal \ (1,1)` = 斜め ＼
- `diagonal / (1,-1)` = 斜め ／

## 🐛 よくある問題

### 問題1: 開いた三を検出しない

**症状:**
```
[TSS-DETAIL]   dir(1,0): left=0, right=2, total=3
[TSS-DETAIL]   left_pos(4,7)=0, right_pos(8,7)=1  ← 左端が開いていない
```

**原因:**
- 左端が盤外または相手の石
- パターンが`X●●●_`の形（片側だけ開いている）

**解決:** これは「開いた三」ではなく「閉じた三」なので正常

### 問題2: 検出したのに別の場所に打つ

**症状:**
```
[TSS] ★★★ Open-three DETECTED at (5,7), dir=horizontal(1,0) ★★★
...
[AI] Defended opponent's open three / split three!
盤面: ○が(5,6)に置かれた  ← (5,7)ではない！
```

**原因:**
- 複数の開いた三が見つかった
- 最初に見つかったものを採用（ループの順序による）

**解決:** より高い脅威（開いた四など）を優先するロジックは既に実装済み

### 問題3: ファイルが作成されない

**症状:**
```
[Learning] ERROR: Could not open learning_data.dat for writing!
Error details: Permission denied
```

**原因:**
- 書き込み権限がない
- ウイルス対策ソフトがブロックしている
- ファイルが他のプログラムで開かれている

**解決:**
1. 管理者権限で実行
2. ウイルス対策ソフトの除外設定
3. 別のディレクトリで実行

## 📝 報告すべき情報

問題が解決しない場合、以下の情報を報告してください：

1. **完全な出力ログ**（プログラム起動からゲーム終了まで）
2. **盤面のスクリーンショット**
3. **カレントディレクトリ**（`[Learning] Current directory:`の出力）
4. **ファイルの有無**（`dir learning_data.dat`の結果）
5. **config.txtの内容**

## ✅ 正常動作の確認

以下の出力が表示されれば正常：

### TSS検出の成功
```
[TSS] ★★★ Open-three DETECTED at (x,y), dir=horizontal ★★★
[AI] Defended opponent's open three / split three!
```

### 学習ファイルの保存成功
```
[AI] Game already finished! Winner: X
[Learning] finish_game_record called with result=X
[Learning] Current directory: C:\...
[Learning] File opened successfully (handle=0x...)
[Learning] Wrote stats: 1 records
[Learning] Learning data saved successfully
```

---

修正内容：
1. ✅ `debug_level=3`で詳細なTSS検出ログを追加
2. ✅ プレイヤーが勝った時に`finish_game_record()`を呼ぶ処理を追加
3. ✅ 方向名（horizontal/vertical/diagonal）を表示
4. ✅ 左右のカウント情報を表示

これらの修正により、TSSがなぜ検出に失敗するか、または間違った位置を選ぶかが明確になります。
