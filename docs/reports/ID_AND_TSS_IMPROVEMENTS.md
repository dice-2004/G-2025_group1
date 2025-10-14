# ID法探索およびTSS検出の改善レポート

## 実装日
2025年1月

## 概要
このレポートは、反復深化（ID）探索の時間管理強化とTSS（Threat Space Search）の詳細なデバッグ機能の実装について説明します。

## 1. ID法探索の改善

### 1.1 厳格な時間制限の実装

#### 目的
探索中に絶対的な時間制限を超えた場合、その深さの探索中であっても必ず処理を停止する。

#### 実装内容

**グローバル変数の追加** (`logics/main_engine.c`)
```c
static clock_t g_search_deadline;     // 絶対的な探索終了時刻
static int g_time_limit_exceeded = 0; // 時間制限超過フラグ
static int g_early_cutoff_threshold = 8000; // 早期カットオフ閾値
```

**alpha_beta関数の時間チェック**
- 各ノード探索の開始時に現在時刻をチェック
- `g_search_deadline`を超えた場合、即座に探索を中断
- `g_time_limit_exceeded`フラグをセットして上位関数に通知

```c
// 時間制限の厳格なチェック（探索途中でも必ず停止）
if (g_search_deadline > 0) {
    clock_t current = clock();
    if (current >= g_search_deadline) {
        g_time_limit_exceeded = 1;
        return 0;  // 時間切れ、評価値は不定だが即座に返す
    }
}
```

**iterative_deepening関数での時間管理**
- 各深さの開始前に残り時間を計算
- `g_search_deadline`を設定（絶対的な終了時刻）
- 深さ完了後に`g_time_limit_exceeded`フラグをチェック
- 時間超過時は前の深さの結果を返す

```c
// 厳格な時間制限を設定（この深さの探索中に絶対に超えてはいけない時間）
g_time_limit_exceeded = 0;
if (time_limit_ms > 0) {
    double remaining_ms = time_limit_ms - elapsed;
    if (remaining_ms <= 0) {
        break;
    }
    g_search_deadline = start_time + (clock_t)((time_limit_ms * CLOCKS_PER_SEC) / 1000);
} else {
    g_search_deadline = 0;  // 時間制限なし
}
```

### 1.2 深さごとの手のソート

#### 目的
各深さの探索完了後、評価値で手をソートし、次の深さでより効率的な枝刈りを実現する。

#### 実装内容

**データ構造の追加**
```c
typedef struct {
    int x, y;
    int score;
} ScoredMove;
```

**ソート関数**
```c
static int compare_scored_moves(const void *a, const void *b) {
    const ScoredMove *ma = (const ScoredMove *)a;
    const ScoredMove *mb = (const ScoredMove *)b;
    return mb->score - ma->score;  // 降順
}
```

**alpha_beta_search関数の改善**
- 全ての手に評価値を割り当て
- `qsort()`を使用して評価値の高い順にソート
- ソート後の最良手を返す
- デバッグモードで上位5手を表示

```c
// 次の深さのために手をソート（評価値の高い順）
qsort(scored_moves, move_count, sizeof(ScoredMove), compare_scored_moves);

if (g_config.debug_level >= 2) {
    printf("[Alpha-Beta] Moves sorted by score (top 5):\n");
    for (int i = 0; i < move_count && i < 5; i++) {
        printf("  %d. (%d,%d) score=%d\n", i+1,
               scored_moves[i].x, scored_moves[i].y, scored_moves[i].score);
    }
}
```

### 1.3 早期カットオフの実装

#### 目的
一定の基準以上の評価値を持つ手が見つかった時点で、次の深さへの探索を行わずに即座に返す。

#### 実装内容

**カットオフ閾値の設定**
```c
static int g_early_cutoff_threshold = 8000; // デフォルト: open-four相当
```

**iterative_deepening関数でのチェック**
```c
// 早期カットオフ: 十分に高い評価値が得られたら次の深さに進まずに返す
if (g_early_cutoff_threshold > 0 && score >= g_early_cutoff_threshold) {
    if (g_config.debug_level >= 1) {
        printf("[ID] Early cutoff triggered! score=%d >= threshold=%d\n",
               score, g_early_cutoff_threshold);
        printf("[ID] Returning without deeper search.\n");
    }
    break;
}
```

**デバッグ出力**
- カットオフ閾値の表示
- カットオフ発生時の通知

### 1.4 効果

1. **時間管理の精度向上**
   - 探索中の時間超過を確実に検出
   - 時間制限を超えた深さの結果を破棄し、信頼性の高い前の深さの結果を返す

2. **探索効率の向上**
   - 手のソートにより、良い手が優先的に探索される
   - Alpha-Beta枝刈りの効果が向上

3. **応答速度の改善**
   - 早期カットオフにより、明らかに有利な手を素早く返せる
   - 不必要な深い探索を回避

## 2. TSS検出のデバッグ機能強化

### 2.1 目的
ユーザーから「両空き3（open-three）」と「間空き3（split-three）」が正しく検出されていないとの報告があったため、詳細なデバッグ出力を追加。

### 2.2 実装内容

#### has_open_three関数のデバッグ出力

**パターン表示**
```c
if (g_config.debug_level >= 3) {
    printf("[TSS Debug] Checking open-three at (%d,%d), direction=(%d,%d): ", x, y, dx, dy);
    for (int i = 0; i < 7; i++) {
        if (line[i] == -1) printf("X");
        else if (line[i] == STONE_SPACE) printf("_");
        else if (line[i] == color) printf("●");
        else printf("○");
    }
    printf("\n");
}
```

**検出成功時の通知**
```c
if (g_config.debug_level >= 2) {
    printf("[TSS] Open-three detected at (%d,%d), direction=(%d,%d)\n", x, y, dx, dy);
}
```

#### has_split_three関数のデバッグ出力

**パターン別の検出通知**
```c
if (g_config.debug_level >= 2) {
    printf("[TSS] Split-three detected (pattern 1: _●●_●_) at (%d,%d), direction=(%d,%d)\n",
           x, y, dx, dy);
}
```

```c
if (g_config.debug_level >= 2) {
    printf("[TSS] Split-three detected (pattern 2: _●_●●_) at (%d,%d), direction=(%d,%d)\n",
           x, y, dx, dy);
}
```

#### find_strong_attack_move関数のデバッグ出力

**探索開始の通知**
```c
if (g_config.debug_level >= 2) {
    printf("[TSS] Searching for strong attack moves (color=%d)...\n", color);
}
```

**探索結果の通知**
```c
if (g_config.debug_level >= 2) {
    printf("[TSS] No strong attack move found.\n");
}
```

### 2.3 デバッグレベルの使い方

**config.txt**で設定可能：
```
debug_level=0  # デバッグ出力なし
debug_level=1  # 基本情報のみ（デフォルト）
debug_level=2  # TSS検出の成功/失敗を表示
debug_level=3  # 全パターンマッチングの詳細を表示
```

### 2.4 活用方法

1. **問題の特定**
   - `debug_level=3`に設定して、各位置でのパターンマッチングを確認
   - どのパターンが検出されているか視覚的に確認可能

2. **動作検証**
   - `debug_level=2`で、実際に検出された脅威を確認
   - 期待される位置で検出が行われているか確認

3. **パフォーマンス測定**
   - `debug_level=1`で、基本的な探索情報のみ表示
   - 本番環境では`debug_level=0`を推奨

## 3. ビルドと実行

### ビルド
```cmd
.\build.bat
```

### 実行
```cmd
gomoku_new.exe
```

### デバッグモードでの実行
1. `config.txt`を編集：
   ```
   debug_level=2
   ```

2. プログラムを実行してTSSの動作を観察

## 4. 今後の拡張

### 学習機能との統合
- `g_early_cutoff_threshold`を学習データから自動調整
- ゲーム結果に基づいて最適な閾値を学習

### ソート順序の最適化
- 置換表（TT）のヒット率を考慮したソート
- 前の深さの最良手を最優先で探索

### TSSパターンの拡張
- より複雑な脅威パターンの追加
- 連続攻撃（四三、三三など）の検出

## 5. 既知の制限事項

1. **時間計測の精度**
   - `clock()`関数の精度に依存
   - Windows環境では約15msの精度

2. **早期カットオフの閾値**
   - 現在は固定値（8000）
   - 局面に応じた動的調整が必要

3. **デバッグ出力のオーバーヘッド**
   - `debug_level=3`では大量の出力が発生
   - パフォーマンスに影響する可能性あり

## 6. まとめ

今回の実装により、以下の改善が達成されました：

1. ✅ **厳格な時間管理**: 探索中でも確実に時間制限を守る
2. ✅ **手のソート**: 各深さで評価値順にソートし、枝刈り効率を向上
3. ✅ **早期カットオフ**: 有利な手を素早く返す
4. ✅ **TSSデバッグ**: 詳細なパターンマッチング情報を出力

これらの機能により、AI探索の精度と効率が大幅に向上しました。
