# 時間制限機能の実行例とテスト

**検証日**: 2025年10月15日
**目的**: 通常時間制限と厳格時間制限の実際の動作を確認する

---

## 1. テスト環境

### 設定ファイル: config.txt

```plaintext
# 基本設定
SEARCH_DEPTH=6
TRANSPOSITION_TABLE_SIZE=262144
DEBUG_MODE=0

# 時間制限設定（テスト用）
TIMEOUT_MS=10000              # 通常時間制限: 10秒
STRICT_TIME_LIMIT_MS=20000    # 厳格時間制限: 20秒
ID_TIME_THRESHOLD=0.8         # 閾値: 80%

# 学習設定
LEARNING_MODE=PRACTICE
ITERATIVE_DEEPENING=1

# デバッグ設定（詳細ログ出力）
DEBUG_LEVEL=1
```

---

## 2. 期待される動作

### ケース1: 通常時間制限での停止

**想定シナリオ**:
```
0秒:    深さ1開始
2秒:    深さ1完了 → 深さ2へ
5秒:    深さ2完了 → 深さ3へ
7秒:    深さ3完了 → 深さ4へ
8秒:    深さ4完了
        8000ms > 10000ms × 0.8 = 8000ms
        → 深さ5には進まない
```

**期待されるログ出力**:
```
[Iterative Deepening] Starting search
  Max depth: 6
  Time limit: 10000 ms
  Strict time limit: 20000 ms (configured separately)
  Time threshold: 80%
  Early cutoff threshold: 8000

[ID] Depth 1 completed: (7, 7) score=XXX time=X.XXXs
[ID] Depth 2 completed: (X, X) score=XXX time=X.XXXs
[ID] Depth 3 completed: (X, X) score=XXX time=X.XXXs
[ID] Depth 4 completed: (X, X) score=XXX time=X.XXXs (elapsed: 8XXXms / 10000ms = XX.X%)

[ID] Time threshold reached (8XXXms / 10000ms = 8X.X%), stopping before depth 5.
[ID] Returning result from depth 4.

[ID] Search completed
  Completed depth: 4
  Best move: (X, X)
  Best score: XXX
  Total time: 8.XXX sec
```

**検証ポイント**:
- ✅ 深さ4で停止
- ✅ 深さ5には進まない
- ✅ 8秒前後で終了
- ✅ 深さ4の結果を返す

---

### ケース2: 厳格時間制限での停止（探索中）

**想定シナリオ（深度探索が長引く場合）**:
```
0秒:    深さ1開始
2秒:    深さ1完了 → 深さ2へ
5秒:    深さ2完了 → 深さ3へ
7秒:    深さ3完了 → 深さ4へ
7秒:    7000ms < 8000ms → 深さ4開始
...     （深さ4の探索が長引く）
19.5秒: 深さ4の途中
20秒:   厳格な制限到達 → 即座に停止
```

**期待されるログ出力**:
```
[Iterative Deepening] Starting search
  Max depth: 6
  Time limit: 10000 ms
  Strict time limit: 20000 ms (configured separately)
  Time threshold: 80%

[ID] Depth 1 completed: (7, 7) score=XXX time=X.XXXs
[ID] Depth 2 completed: (X, X) score=XXX time=X.XXXs
[ID] Depth 3 completed: (X, X) score=XXX time=X.XXXs
[ID] Depth 4 started...
[Root] Time limit exceeded at move XX/XX
（または）
[Time Limit] Exceeded in Max loop at move XX/XX

[ID] Strict time limit exceeded during depth 4 search (20XXXms / 20000ms strict).
[ID] Discarding depth 4 result, returning depth 3 result: (X, X) score=XXX

[ID] Search completed
  Completed depth: 3
  Best move: (X, X)
  Best score: XXX
  Total time: 20.XXX sec
```

**検証ポイント**:
- ✅ 深さ4の途中で停止
- ✅ 20秒前後で終了（絶対に超えない）
- ✅ 深さ3の結果を返す（深さ4は破棄）
- ✅ 時間超過フラグが検出される

---

### ケース3: 厳格時間制限での停止（深度開始前）

**設定変更**:
```plaintext
TIMEOUT_MS=10000
STRICT_TIME_LIMIT_MS=15000
ID_TIME_THRESHOLD=0.7  # 70%を超えたら停止
```

**想定シナリオ**:
```
0秒:    深さ1開始
2秒:    深さ1完了 → 深さ2へ進むか判定
        2000ms > 7000ms → FALSE（通常閾値内） → 進む
5秒:    深さ2完了 → 深さ3へ進むか判定
        5000ms > 7000ms → FALSE（通常閾値内） → 進む
7秒:    深さ3完了 → 深さ4へ進むか判定
        7000ms > 7000ms → FALSE（境界値） → 進む
9秒:    深さ4完了 → 深さ5へ進むか判定
        9000ms > 7000ms → TRUE（通常閾値超過）
        ✅ ここで停止（深さ5には進まない）

重要: 厳格制限(15秒)には達していないが、通常閾値(7秒)を超えたため停止
```

**期待されるログ出力**:
```
[ID] Depth 1 completed: (7, 7) score=XXX time=X.XXXs
[ID] Depth 2 completed: (X, X) score=XXX time=X.XXXs
[ID] Depth 3 completed: (X, X) score=XXX time=X.XXXs
[ID] Depth 4 completed: (X, X) score=XXX time=XX.XXXs (elapsed: 9XXXms / 10000ms = 9X.X%)

[ID] Time threshold reached (9XXXms / 10000ms = 9X.X%), stopping before depth 5.
[ID] Returning result from depth 4.

[ID] Search completed
  Completed depth: 4
  Best move: (X, X)
  Best score: XXX
  Total time: 14.XXX sec
```

**検証ポイント**:
- ✅ 深さ5開始前に停止
- ✅ 9秒前後で終了（通常閾値で停止）
- ✅ 深さ4の結果を返す
- ✅ 厳格制限には達していないが通常閾値で停止

---

## 3. 実行方法

### ステップ1: プログラムのコンパイル
```batch
build.bat
```

### ステップ2: 設定ファイルの編集
`config.txt`を上記の設定に変更

### ステップ3: プログラムの実行
```batch
gomoku_new.exe
```

### ステップ4: ログの確認
`DEBUG_LEVEL=1`以上にすると、詳細なログが出力されます。

---

## 4. 検証チェックリスト

### 通常時間制限のチェック

- [ ] 8秒前後で深さ5に進まず停止する
- [ ] "Time threshold reached"のメッセージが表示される
- [ ] 深さ4の結果が返される
- [ ] 総時間が8秒前後

### 厳格時間制限のチェック（探索中）

- [ ] 20秒前後で強制停止する
- [ ] "Strict time limit exceeded during depth X search"のメッセージが表示される
- [ ] 時間超過した深度の結果は破棄される
- [ ] 前の深度の結果が返される
- [ ] 総時間が20秒前後（絶対に超えない）

### 厳格時間制限のチェック（開始前）

- [ ] 深度開始前に厳格制限に到達する
- [ ] "Strict time limit reached before depth X"のメッセージが表示される
- [ ] 完了した最後の深度の結果が返される
- [ ] 総時間が20秒前後

### 一般的なチェック

- [ ] 厳格制限が通常制限より優先される
- [ ] 設定ファイルの値が正しく読み込まれる
- [ ] デバッグレベルに応じて適切なログが出力される

---

## 5. トラブルシューティング

### 問題1: ログが出力されない

**原因**: DEBUG_LEVELが0になっている

**解決策**:
```plaintext
DEBUG_LEVEL=1  # または 2, 3
```

### 問題2: 時間制限が無視される

**原因**: ITERATIVE_DEEPENINGがOFFになっている

**解決策**:
```plaintext
ITERATIVE_DEEPENING=1
```

### 問題3: 厳格制限が機能しない

**原因**: STRICT_TIME_LIMIT_MSが0になっている

**解決策**:
```plaintext
STRICT_TIME_LIMIT_MS=20000  # 0以外の値を設定
```

---

## 6. 推奨テスト設定

### クイックテスト（短時間で確認）
```plaintext
TIMEOUT_MS=3000
STRICT_TIME_LIMIT_MS=5000
ID_TIME_THRESHOLD=0.8
DEBUG_LEVEL=1
```

### 標準テスト（通常の使用を想定）
```plaintext
TIMEOUT_MS=10000
STRICT_TIME_LIMIT_MS=20000
ID_TIME_THRESHOLD=0.8
DEBUG_LEVEL=1
```

### 詳細テスト（デバッグ用）
```plaintext
TIMEOUT_MS=10000
STRICT_TIME_LIMIT_MS=20000
ID_TIME_THRESHOLD=0.8
DEBUG_LEVEL=3  # 超詳細ログ
```

---

**最終更新**: 2025年10月15日
**検証者**: GitHub Copilot
