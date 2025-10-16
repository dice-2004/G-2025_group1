# Early Cutoff機能の検証と改善提案

## 1. 問題の概要

### 1.1 発見された問題

ユーザーログから以下の動作が観察されました：

```
[ID] Depth 5 completed: (3, 4) score=10376 time=2.454s (elapsed: 2687ms / 10000ms = 26.9%)
[ID] Early cutoff triggered! score=10376 >= threshold=8000
[ID] Returning without deeper search.
```

**ユーザーの懸念**:
> 不完全な探索の手を返しているように見える

### 1.2 初期分析結果

コードを確認した結果、以下が判明：

1. ✅ **深度5の探索は完全に完了している**（2.454秒かけて完了）
2. ✅ 深度5の結果は**完全な探索結果**である
3. ⚠️ しかし、Early cutoffの閾値（8000）が適切かどうかは疑問
4. ⚠️ 時間に余裕がある場合でも深度6に進んでいない

---

## 2. Early Cutoffの現在の実装

### 2.1 コードの動作（main_engine.c lines 561-569）

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

### 2.2 実行順序

```
1. 深度Nの探索を完全に実行
2. 深度Nの結果を取得（完全な探索結果）
3. スコアがthresholdを超えているかチェック
4. 超えていたら深度N+1には進まない
5. 深度Nの結果を返す
```

**重要**: 深度Nの探索は**完了している**ため、返される結果は**完全な探索結果**である。

---

## 3. 問題点の詳細分析

### 3.1 Early Cutoffの意図

Early cutoffには以下の目的があると考えられる：

1. **確実な勝ちが見つかった場合、それ以上探索しない**
2. **時間を節約し、他の局面に時間を使う**

### 3.2 現在の閾値の問題

**設定**: `EARLY_CUTOFF_THRESHOLD=8000`

**評価値の基準**（evaluation.c）:
```c
SCORE_FIVE      = 100000    // 五連（確定勝ち）
SCORE_FOUR      = 10000     // 四連（次に五連確定）
SCORE_OPEN_THREE = 5000     // オープン三連（強力）
SCORE_THREE     = 1000      // 三連
```

**問題点**:

| 状況 | スコア | 閾値8000との関係 | 問題 |
|------|--------|-----------------|------|
| 五連発見 | 100000 | ✅ 8000超過 → カットオフ | 正しい（確定勝ち） |
| 四連発見 | 10000+ | ✅ 8000超過 → カットオフ | **問題あり**（次で勝てるが確認しない） |
| オープン三連×2 | ~10000 | ✅ 8000超過 → カットオフ | **問題あり**（相手も対応可能） |
| オープン三連 | 5000 | ❌ 8000未満 → 続行 | 正しい |
| 三連×3 | ~3000 | ❌ 8000未満 → 続行 | 正しい |

**結論**:
- **閾値8000は中途半端**
- 四連（10000）を見つけても、**確実な勝ちかどうか不明**
- オープン三連複数でもカットオフしてしまう

### 3.3 ログの具体例での問題

```
Depth 5: score=10376 → Early cutoff
```

**分析**:
- `10376`は四連レベルのスコア
- しかし、これは**深度5での評価**
- **深度6で相手の反撃がある可能性**を確認していない
- 時間は2.687秒/10000ms = **26.9%しか使っていない**
- **まだ7秒以上余裕がある**のに探索を打ち切っている

**問題**:
- 時間に余裕があるのに探索を打ち切っている
- より深い読みで、相手の反撃を見逃している可能性

---

## 4. 改善提案

### 4.1 提案A: Early Cutoffの閾値を引き上げる

**変更**: `EARLY_CUTOFF_THRESHOLD = 50000` （五連の半分）

**根拠**:
- 確実な勝ち（五連、または複数の四連）のみでカットオフ
- 単一の四連（10000）ではカットオフしない
- より慎重な判断

**メリット**:
- ✅ 誤った早期終了を防ぐ
- ✅ より深い読みで確実性を高める

**デメリット**:
- ⚠️ Early cutoffの効果が薄れる

### 4.2 提案B: Early Cutoffを時間との兼ね合いで判断

**変更**: 時間に余裕がある場合はカットオフしない

```c
// 早期カットオフ: 時間的余裕がない場合のみ
if (g_early_cutoff_threshold > 0 && score >= g_early_cutoff_threshold) {
    // 時間の使用率をチェック
    double time_usage = (time_limit_ms > 0) ? (elapsed / time_limit_ms) : 0.0;

    // 時間を50%以上使っている場合のみカットオフ
    if (time_usage >= 0.5) {
        if (g_config.debug_level >= 1) {
            printf("[ID] Early cutoff triggered! score=%d >= threshold=%d (time usage: %.1f%%)\n",
                   score, g_early_cutoff_threshold, time_usage * 100);
        }
        break;
    } else {
        if (g_config.debug_level >= 2) {
            printf("[ID] High score (%d) but time remaining (%.1f%%), continuing search.\n",
                   score, (1.0 - time_usage) * 100);
        }
    }
}
```

**根拠**:
- 時間に余裕があるなら、より深く探索すべき
- 時間がない場合のみ、高スコアで妥協する

**メリット**:
- ✅ 時間を有効活用
- ✅ より確実な手を選べる
- ✅ Early cutoffの本来の目的（時間節約）に合致

**デメリット**:
- なし

### 4.3 提案C: Early Cutoffと勝ち判定を統合

**変更**: 五連検出の閾値と統一

```c
// 現在の勝ち判定（line 571-577）
if (score > SCORE_FIVE / 2) {  // 50000
    printf("[ID] Winning move found! (score=%d)\n", score);
    break;
}

// Early cutoffを削除し、勝ち判定のみに統一
```

**根拠**:
- Early cutoff（8000）と勝ち判定（50000）が重複
- 勝ち判定で十分
- コードの複雑性を減らす

**メリット**:
- ✅ シンプルなロジック
- ✅ 誤った早期終了を防ぐ
- ✅ 保守性向上

**デメリット**:
- ⚠️ Early cutoffの機能が完全に失われる

---

## 5. 推奨される改善案

### 5.1 最終推奨: **提案B + 閾値調整**

**理由**:
1. 時間を有効活用できる
2. Early cutoffの本来の目的（時間節約）に合致
3. 柔軟性が高い

**実装**:

```c
// 早期カットオフ: 十分に高い評価値 + 時間的余裕がない場合のみ
if (g_early_cutoff_threshold > 0 && score >= g_early_cutoff_threshold) {
    double time_usage = (time_limit_ms > 0) ? (elapsed / time_limit_ms) : 0.0;

    // 時間を50%以上使っている、または確実な勝ち（50000以上）の場合
    if (time_usage >= 0.5 || score >= SCORE_FIVE / 2) {
        if (g_config.debug_level >= 1) {
            if (score >= SCORE_FIVE / 2) {
                printf("[ID] Winning move found! score=%d\n", score);
            } else {
                printf("[ID] Early cutoff triggered! score=%d >= threshold=%d (time usage: %.1f%%)\n",
                       score, g_early_cutoff_threshold, time_usage * 100);
            }
            printf("[ID] Returning without deeper search.\n");
        }
        break;
    } else {
        if (g_config.debug_level >= 2) {
            printf("[ID] High score (%d) but time remaining (%.1f%%), continuing search.\n",
                   score, (1.0 - time_usage) * 100);
        }
    }
}
```

**閾値の推奨値**: `EARLY_CUTOFF_THRESHOLD = 15000`
- 四連（10000）よりやや高い
- 複数の強い手がある場合に反応

### 5.2 config.txtへの追加項目

```plaintext
# 早期カットオフの時間閾値（0.0～1.0）
# この割合以上の時間を使った場合のみEarly Cutoffを適用
# 0.0 = 常に適用、1.0 = 適用しない
EARLY_CUTOFF_TIME_THRESHOLD=0.5
```

---

## 6. テストケース

### 6.1 修正前の動作（現在）

```
設定: TIMEOUT_MS=10000, EARLY_CUTOFF_THRESHOLD=8000

結果:
- 深度5完了: 2.687秒（26.9%）、score=10376
- Early cutoff発動
- 深度6に進まない
- 時間が7秒以上余っている
```

**問題**: 時間に余裕があるのに探索打ち切り

### 6.2 修正後の期待動作

```
設定: TIMEOUT_MS=10000, EARLY_CUTOFF_THRESHOLD=15000,
      EARLY_CUTOFF_TIME_THRESHOLD=0.5

ケース1: 高スコア + 時間余裕あり
- 深度5完了: 2.687秒（26.9%）、score=10376
- 時間余裕あり（26.9% < 50%）
- 閾値未満（10376 < 15000）
- 深度6に進む ✅

ケース2: 高スコア + 時間余裕なし
- 深度5完了: 6.5秒（65%）、score=16000
- 時間余裕なし（65% >= 50%）
- 閾値超過（16000 >= 15000）
- Early cutoff発動 ✅

ケース3: 確実な勝ち
- 深度3完了: 1.0秒（10%）、score=55000
- 確実な勝ち（55000 >= 50000）
- 即座にカットオフ ✅
```

---

## 7. 実装の優先度

| 項目 | 優先度 | 理由 |
|------|--------|------|
| 時間閾値の追加 | **高** | 時間を有効活用、誤った早期終了を防ぐ |
| Early cutoff閾値の調整 | 中 | より適切な判断基準 |
| 勝ち判定との統合 | 低 | コード整理（機能的な問題なし） |

---

## 8. 結論

### 8.1 現在の実装の評価

- ✅ **「不完全な探索」ではない** - 深度5は完全に探索済み
- ⚠️ **しかし、時間を有効活用していない** - 余裕があるのに打ち切り
- ⚠️ **閾値が中途半端** - より高い値にすべき

### 8.2 推奨される改善

1. **時間閾値の追加** - 時間に余裕がある場合は探索を続ける
2. **Early cutoff閾値の引き上げ** - 15000程度
3. **デバッグログの強化** - 判断理由を明示

### 8.3 期待される効果

- ✅ 時間の有効活用（余裕がある場合はより深く探索）
- ✅ より確実な手の選択
- ✅ Early cutoffの本来の目的に合致（時間節約）
- ✅ 誤った早期終了の防止

---

**作成日**: 2025年10月15日
**検証対象**: `logics/main_engine.c` lines 561-569
**関連ファイル**: `config.h`, `config.c`, `evaluation.h`
