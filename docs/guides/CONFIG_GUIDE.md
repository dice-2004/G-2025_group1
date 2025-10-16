# Config.txt 設定ガイド

## 目次
- [概要](#概要)
- [基本設定](#基本設定)
- [時間管理設定](#時間管理設定)
- [学習モード設定](#学習モード設定)
- [デバッグ設定](#デバッグ設定)
- [推奨設定プリセット](#推奨設定プリセット)
- [設定の読み込み](#設定の読み込み)

---

## 概要

`config.txt`は五目並べAIの動作を制御する設定ファイルです。このファイルを編集することで、AIの強さ、速度、学習機能、デバッグ出力などを柔軟にカスタマイズできます。

**ファイルの場所**: プロジェクトルート `/config.txt`

**読み込みタイミング**: プログラム起動時に一度だけ読み込まれます。設定変更後は再起動が必要です。

---

## 基本設定

### SEARCH_DEPTH（探索深度）

**概要**: AIが先読みする手数を設定します。深いほど強いですが、時間がかかります。

```
SEARCH_DEPTH=4
```

**設定範囲**: 1～8（推奨: 3～6）

**目安**:
- **深さ1**: 瞬時（初級レベル、初心者向け）
- **深さ2-3**: ~200ms（中級レベル、練習相手として最適）
- **深さ4-5**: ~1-5秒（上級レベル、通常の対局用）
- **深さ6-8**: ~10-60秒（大会レベル、最強設定）

**推奨値**:
- 練習モード: `3` または `4`
- 本番モード: `5` または `6`
- 大会モード: `6` または `7`

**注意事項**:
- `ITERATIVE_DEEPENING=1`の場合、この値は「最大深度」として機能します
- `TIMEOUT_MS`と組み合わせることで、時間内に到達可能な深度まで探索します

---

### TRANSPOSITION_TABLE_SIZE（置換表サイズ）

**概要**: 探索済みの局面を記憶するテーブルのサイズです。大きいほど高速化しますが、メモリを消費します。

```
TRANSPOSITION_TABLE_SIZE=262144
```

**設定範囲**: 2の累乗（推奨: 65536～1048576）

**メモリ使用量**:
- `65536` = 約1MB（省メモリ）
- `262144` = 約4MB（推奨、バランス型）
- `524288` = 約8MB（高性能）
- `1048576` = 約16MB（最高性能）

**推奨値**:
- 低スペックPC: `65536`
- 通常PC: `262144`
- 高スペックPC: `524288` または `1048576`

**効果**:
- 同じ局面を再探索しないため、探索が大幅に高速化
- 深い探索ほど効果が大きい

---

## 時間管理設定

### TIMEOUT_MS（タイムアウト）

**概要**: 1手あたりの最大思考時間をミリ秒で設定します。

```
TIMEOUT_MS=10000
```

**設定範囲**: 0～無制限（推奨: 5000～30000）

**目安**:
- `0`: 無制限（`SEARCH_DEPTH`まで必ず探索）
- `5000`: 5秒（スピード重視）
- `10000`: 10秒（標準、推奨）
- `20000`: 20秒（慎重派）
- `30000`: 30秒（大会用）

**推奨値**:
- 練習モード: `5000` または `10000`
- 本番モード: `10000` または `15000`
- 大会モード: `20000` または `30000`

**注意事項**:
- `ITERATIVE_DEEPENING=1`の場合、時間内に複数の深度を探索します
- `TIMEOUT_MS=0`かつ`ITERATIVE_DEEPENING=0`の場合、`SEARCH_DEPTH`まで必ず探索します

---

### ID_TIME_THRESHOLD（反復深化時間制限閾値）

**概要**: 次の深度探索に進むかどうかを判断する閾値です。時間切れを防ぎます。

```
ID_TIME_THRESHOLD=0.8
```

**設定範囲**: 0.0～1.0（推奨: 0.7～0.9）

**動作**:
- この値 × `TIMEOUT_MS` の時間に達したら、次の深度に進みません
- 例: `ID_TIME_THRESHOLD=0.8`、`TIMEOUT_MS=10000` → 8秒時点で次の深度に進まない

**設定例**:
- `0.7` = 70%の時点でストップ（安全重視、時間切れを確実に防ぐ）
- `0.8` = 80%の時点でストップ（推奨、バランス型）
- `0.9` = 90%の時点でストップ（積極的、深く探索したい）

**推奨値**:
- 練習モード: `0.8` または `0.9`
- 本番モード: `0.7` または `0.8`
- 大会モード: `0.7`（時間切れ厳禁）

**適用条件**:
- `ITERATIVE_DEEPENING=1`の場合のみ有効

---

### STRICT_TIME_LIMIT_MS（厳格な時間制限）

**概要**: 探索中に絶対に超えてはいけない時間制限を設定します。

```
STRICT_TIME_LIMIT_MS=0
```

**設定範囲**: 0～無制限（推奨: 0 または `TIMEOUT_MS - 2000`）

**動作**:
- `0`: `TIMEOUT_MS`と同じ制限を使用（デフォルト）
- `>0`: `TIMEOUT_MS`とは独立した厳格な制限時間

**使用例**:
```
# 例1: 通常10秒、絶対8秒で停止
TIMEOUT_MS=10000
STRICT_TIME_LIMIT_MS=8000

# 例2: 通常15秒、絶対12秒で停止
TIMEOUT_MS=15000
STRICT_TIME_LIMIT_MS=12000
```

**推奨値**:
- 練習モード: `0`（不要）
- 本番モード: `TIMEOUT_MS - 2000`（安全マージン2秒）
- 大会モード: `TIMEOUT_MS - 3000`（安全マージン3秒）

**メリット**:
- ターン制ゲームで時間切れによる失格を防ぐ
- 制限時間ぎりぎりまで探索しつつ、安全マージンを確保

---

### EARLY_CUTOFF_TIME_THRESHOLD（早期カットオフ時間閾値）

**概要**: 高スコア（勝利確定手など）が見つかった時に、探索を早期終了するかを判断する閾値です。

```
EARLY_CUTOFF_TIME_THRESHOLD=0.5
```

**設定範囲**: 0.0～1.0（推奨: 0.3～0.7）

**動作**:
- 時間使用率がこの閾値を超えている場合、高スコア（例: 五並び）が見つかったら即座に探索終了
- 時間使用率が閾値未満の場合、高スコアでも探索を継続（もっと良い手があるかも）

**設定例**:
- `0.0` = 常にEarly cutoffを適用（高スコアなら即座に終了、時間節約重視）
- `0.5` = 時間を50%以上使った場合のみ適用（推奨、バランス型）
- `0.8` = ほぼ時間を使い切るまで探索（慎重型）
- `1.0` = Early cutoffを実質無効化（常に最後まで探索）

**推奨値**:
- 序盤・中盤: `0.5` または `0.6`（時間を有効活用）
- 終盤: `0.3` または `0.4`（高スコアなら早期終了）

**ゲーム段階別推奨設定**:
| ゲーム段階 | 推奨値 | 理由 |
|-----------|--------|------|
| 序盤（1-10手） | 0.6-0.7 | 定跡があるので慎重に |
| 中盤（11-30手） | 0.5-0.6 | バランス重視 |
| 終盤（31手-） | 0.3-0.4 | 勝利手があれば即終了 |

---

### 時間管理の3つの閾値の違い

| 項目 | ID_TIME_THRESHOLD | STRICT_TIME_LIMIT_MS | EARLY_CUTOFF_TIME_THRESHOLD |
|------|-------------------|----------------------|------------------------------|
| **判断タイミング** | 各深度の開始前 | 探索中常時 | 高スコア発見時 |
| **判断内容** | 次の深度に進むか | 強制終了するか | 探索を終了するか |
| **目的** | 時間切れを防ぐ | 絶対的な時間制限 | 時間を節約する |
| **効果** | 深度が深くなりすぎない | 確実に時間内に終了 | 明らかな良手なら早く返す |
| **推奨値** | 0.8 | 0 or TIMEOUT-2000 | 0.5 |

---

## 学習モード設定

### LEARNING_MODE（学習モード）

**概要**: AIの学習機能を制御します。

```
LEARNING_MODE=PRACTICE
```

**設定値**:
- `PRACTICE`: 練習モード（学習しながら学習データを参照）
- `PRODUCTION`: 本番モード（学習せず学習データのみ参照）

**詳細**:

#### PRACTICE（練習モード）
- **動作**: 対局結果から学習し、学習データを更新
- **用途**: 練習、トレーニング、AIの強化
- **データ**: `opening_book.csv`と`patterns_extended.csv`を読み書き
- **推奨**: 日常的な練習や対局で使用

#### PRODUCTION（本番モード）
- **動作**: 学習データを参照するのみ（更新しない）
- **用途**: 本番、大会、評価テスト
- **データ**: 既存の学習データのみ読み込み
- **推奨**: 大会や評価時、AIの性能を固定したい時

**推奨設定**:
- 開発・練習中: `PRACTICE`
- 大会・評価時: `PRODUCTION`

**学習データの場所**:
- 定跡データ: `opening_book.csv`
- パターン学習: `patterns_extended.csv`

---

## デバッグ設定

### DEBUG_MODE（デバッグモード）

**概要**: デバッグ用の基本ログを出力します（非推奨、`DEBUG_LEVEL`を使用してください）。

```
DEBUG_MODE=0
```

**設定値**:
- `0`: OFF（推奨）
- `1`: ON（`DEBUG_LEVEL`を使用することを推奨）

**注意**: このオプションは古い設定です。`DEBUG_LEVEL`を使用してください。

---

### DEBUG_LEVEL（詳細デバッグ出力）

**概要**: AIの思考過程を詳細に出力します。レベルが上がるほど詳しくなります。

```
DEBUG_LEVEL=1
```

**設定範囲**: 0～3

**レベル詳細**:

#### DEBUG_LEVEL=0（出力なし）
- **出力内容**: エラーメッセージのみ
- **用途**: 本番、大会
- **推奨**: プログラムの動作確認後、本番運用時

#### DEBUG_LEVEL=1（基本情報）
- **出力内容**:
  - 学習ログ（学習データの読み込み、更新）
  - TSSログ（どのTSSが反応したか）
  - 定跡ログ（どの定跡が選ばれたか）
  - AI決定ログ（最終的にどの手を選んだか）
  - データファイル情報（読み込んだファイル、ゲーム数）
- **用途**: 日常的な対局、AIの動作確認
- **推奨**: 通常使用時のデフォルト設定

**出力例**:
```
[Learning] Loaded 45 games from opening_book.csv
[OpeningBook] Selected game 123 (PRACTICE mode: random)
[AI-TSS] Selected method: Defense Move
[AI] Decision: (7, 7)
[Learning] Updated pattern: Open Three (wins: 15, total: 28, factor: 0.214)
```

#### DEBUG_LEVEL=2（詳細情報）
- **出力内容**:
  - レベル1の全て
  - 各深度の探索結果
  - 評価値の詳細
  - 置換表の統計情報（ヒット率、衝突率）
  - 候補手の評価値リスト
- **用途**: AIの強化、パラメータ調整
- **推奨**: 学習効果の確認、性能チューニング時

**出力例**:
```
[Search] Depth 1: Best move (7,7) score=1200 time=0.05s
[Search] Depth 2: Best move (7,8) score=1500 time=0.15s
[Search] Depth 3: Best move (7,7) score=1800 time=0.45s
[TranspositionTable] Hit rate: 75.3%, Collision rate: 2.1%
[Evaluation] Candidate moves: (7,7)=1800, (8,7)=1500, (6,7)=1200
```

#### DEBUG_LEVEL=3（超詳細）
- **出力内容**:
  - レベル2の全て
  - 全候補手の評価値
  - 枝刈り情報（Alpha-Beta枝刈りの詳細）
  - メモリ使用状況
  - パターンマッチング詳細
- **用途**: バグ修正、アルゴリズム開発
- **推奨**: 開発者向け、問題発生時のデバッグ

**出力例**:
```
[AlphaBeta] Depth 3, alpha=-10000, beta=10000
[AlphaBeta] Trying move (7,7), score=1800
[AlphaBeta] Cutoff at (8,7), beta cutoff
[Pattern] Matched: Open Three at (7,7), base_score=1000, adjusted=1200
[Memory] Transposition table: 45231/262144 entries (17.2%)
```

**推奨設定**:
- 本番・大会: `DEBUG_LEVEL=0`
- 通常対局: `DEBUG_LEVEL=1`
- 学習確認: `DEBUG_LEVEL=2`
- 開発・デバッグ: `DEBUG_LEVEL=3`

---

### ITERATIVE_DEEPENING（反復深化探索）

**概要**: 深さ1から徐々に探索を深める手法です。時間制限への対応や早期の良い手発見に有効です。

```
ITERATIVE_DEEPENING=1
```

**設定値**:
- `0`: OFF（`SEARCH_DEPTH`まで一度に探索）
- `1`: ON（推奨、深さ1から徐々に探索）

**メリット**:
1. **時間制限への対応**: 時間切れ時も前の深度の結果を返せる
2. **早期の良い手発見**: 浅い探索で良い手を早く見つけられる
3. **置換表の効率向上**: 浅い探索の結果が深い探索で再利用される
4. **手の並び替え**: 前の深度の良い手を優先的に探索

**推奨値**:
- 通常: `1`（ON）
- 時間無制限: `0`でも可

**注意事項**:
- `TIMEOUT_MS=0`の場合、ONでもOFFでも同じ結果になります
- `TIMEOUT_MS>0`の場合、ONを強く推奨します

---

## 推奨設定プリセット

### プリセット1: 初心者練習用

```
SEARCH_DEPTH=3
TRANSPOSITION_TABLE_SIZE=65536
DEBUG_MODE=0
TIMEOUT_MS=5000
LEARNING_MODE=PRACTICE
ITERATIVE_DEEPENING=1
DEBUG_LEVEL=1
ID_TIME_THRESHOLD=0.8
STRICT_TIME_LIMIT_MS=0
EARLY_CUTOFF_TIME_THRESHOLD=0.5
```

**特徴**:
- 弱めのAI（深度3）で初心者でも勝ちやすい
- 高速（5秒制限）でテンポよく対局
- 学習機能ONで徐々に強くなる

---

### プリセット2: 通常対局用（推奨）

```
SEARCH_DEPTH=5
TRANSPOSITION_TABLE_SIZE=262144
DEBUG_MODE=0
TIMEOUT_MS=10000
LEARNING_MODE=PRACTICE
ITERATIVE_DEEPENING=1
DEBUG_LEVEL=1
ID_TIME_THRESHOLD=0.8
STRICT_TIME_LIMIT_MS=0
EARLY_CUTOFF_TIME_THRESHOLD=0.5
```

**特徴**:
- バランスの取れた強さ（深度5）
- 標準的な思考時間（10秒）
- 学習機能ONで継続的に成長

---

### プリセット3: 大会用（最強・安全重視）

```
SEARCH_DEPTH=7
TRANSPOSITION_TABLE_SIZE=524288
DEBUG_MODE=0
TIMEOUT_MS=25000
LEARNING_MODE=PRODUCTION
ITERATIVE_DEEPENING=1
DEBUG_LEVEL=0
ID_TIME_THRESHOLD=0.7
STRICT_TIME_LIMIT_MS=22000
EARLY_CUTOFF_TIME_THRESHOLD=0.4
```

**特徴**:
- 最強設定（深度7）
- 長時間思考（25秒、安全マージン3秒）
- 学習OFF（本番モード）で性能固定
- デバッグ出力OFFで高速化
- 時間切れを確実に防ぐ設定

---

### プリセット4: 開発・デバッグ用

```
SEARCH_DEPTH=4
TRANSPOSITION_TABLE_SIZE=262144
DEBUG_MODE=0
TIMEOUT_MS=10000
LEARNING_MODE=PRACTICE
ITERATIVE_DEEPENING=1
DEBUG_LEVEL=3
ID_TIME_THRESHOLD=0.8
STRICT_TIME_LIMIT_MS=0
EARLY_CUTOFF_TIME_THRESHOLD=0.5
```

**特徴**:
- 標準的な強さ（深度4）
- 超詳細ログ（DEBUG_LEVEL=3）でデバッグしやすい
- 学習機能ONでパラメータ調整の効果を確認

---

### プリセット5: 高速テスト用

```
SEARCH_DEPTH=2
TRANSPOSITION_TABLE_SIZE=65536
DEBUG_MODE=0
TIMEOUT_MS=2000
LEARNING_MODE=PRACTICE
ITERATIVE_DEEPENING=1
DEBUG_LEVEL=1
ID_TIME_THRESHOLD=0.8
STRICT_TIME_LIMIT_MS=0
EARLY_CUTOFF_TIME_THRESHOLD=0.3
```

**特徴**:
- 非常に高速（深度2、2秒制限）
- 大量の対局でテストしたい時に最適
- 早期カットオフで更に高速化

---

## 設定の読み込み

### ファイル形式

```
# コメント行（#で始まる行は無視される）
PARAMETER_NAME=value

# 空行もOK

ANOTHER_PARAMETER=value
```

### 読み込み順序

1. プログラム起動時に`config.txt`を探す
2. 見つからない場合、デフォルト値を使用
3. 各パラメータを順番に読み込み
4. 不正な値の場合、エラーメッセージを表示してデフォルト値を使用

### エラー処理

**存在しないパラメータ**: 無視される（警告なし）
**不正な値**: エラーメッセージを表示し、デフォルト値を使用
**ファイルが見つからない**: 全てデフォルト値で起動

### デフォルト値一覧

| パラメータ | デフォルト値 |
|-----------|-------------|
| SEARCH_DEPTH | 4 |
| TRANSPOSITION_TABLE_SIZE | 262144 |
| DEBUG_MODE | 0 |
| TIMEOUT_MS | 10000 |
| LEARNING_MODE | PRACTICE |
| ITERATIVE_DEEPENING | 1 |
| DEBUG_LEVEL | 1 |
| ID_TIME_THRESHOLD | 0.8 |
| STRICT_TIME_LIMIT_MS | 0 |
| EARLY_CUTOFF_TIME_THRESHOLD | 0.5 |

---

## 関連ドキュメント

- **学習機能の詳細**: [TRAINING_GUIDE.md](TRAINING_GUIDE.md)
- **使用方法**: [USAGE_GUIDE.md](USAGE_GUIDE.md)
- **技術詳細**: [../technical/TECHNICAL_DOCUMENT.md](../technical/TECHNICAL_DOCUMENT.md)
- **プロジェクト概要**: [PROJECT_OVERVIEW.md](PROJECT_OVERVIEW.md)

---

## トラブルシューティング

### Q: 設定を変更したのに反映されない
A: プログラムを再起動してください。`config.txt`は起動時に一度だけ読み込まれます。

### Q: AIが時間切れになる
A: `ID_TIME_THRESHOLD`を下げる（例: 0.7）か、`STRICT_TIME_LIMIT_MS`を設定してください。

### Q: AIが弱すぎる/強すぎる
A: `SEARCH_DEPTH`を調整してください（弱く: 2-3、強く: 6-7）。

### Q: 学習が効いていない気がする
A: `DEBUG_LEVEL=2`にして学習ログを確認してください。`LEARNING_MODE=PRACTICE`になっているか確認してください。

### Q: 大量のログが出て見づらい
A: `DEBUG_LEVEL=1`または`0`に下げてください。

### Q: メモリ不足エラーが出る
A: `TRANSPOSITION_TABLE_SIZE`を下げてください（例: 65536）。

---

**更新日**: 2025年10月16日
**バージョン**: v1.0
