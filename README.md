# 五目並べAI v3.3 - 時間管理機能強化版

## 🎯 概要

**対戦するほど強くなる**五目並べAI実装です。

### 🆕 v3.3の新機能

⏱️ **ID時間管理機能** - Iterative Deepeningの時間制限を完全制御
- **時間切れ時に前の深度の結果を返す**（重要な改善！）
- 時間閾値をconfig.txtで簡単に調整（0.7～0.95）
- 探索開始前と完了後の二段階チェック
- 不完全な結果を使わず、確実な結果を返す

### v3.2の機能

🐛 **デバッグ機能** - 探索の詳細を可視化
- デバッグレベル 0-3（OFF/基本/詳細/超詳細）
- 探索統計（ノード数、枝刈り率、TT hit率）
- 盤面表示、評価値詳細

🔄 **Iterative Deepening（反復深化探索）** - 時間制限への対応
- 深さ1から徐々に探索
- 時間制限内で最善手を返す
- 置換表の効率向上
- 早期の良手・詰み手発見

### v3.1の機能

🎛️ **学習モード切替機能** - 練習モードと本番モードを選択可能
- **練習モード（PRACTICE）**: 学習しながら成長
- **本番モード（PRODUCTION）**: 学習済みデータで安定動作

### v3.0の機能

✨ **学習機能** - 対戦を重ねるごとに自動的に強くなります
- パターンスコアの自動最適化
- 序盤定石の自動学習
- 学習データの永続化

## 🚀 クイックスタート

### Windows (build.bat)
```cmd
# ビルド
build.bat

# 実行
gomoku_new.exe
```

### Linux/Mac/MSYS2 (Makefile)
```bash
# ビルド
make all

# 実行
./gomoku

# クリーン
make clean
```

**何度も対戦してAIを育てよう！**

## 📚 ドキュメント

| ファイル | 説明 |
|---------|------|
| **MAKEFILE_GUIDE.md** | 🔧 🆕 Makefile完全ガイド（Linux/Mac対応） |
| **ID_TIME_MANAGEMENT.md** | ⏱️ 🆕 ID時間管理完全ガイド（v3.3） |
| **DEBUG_AND_ID_GUIDE.md** | 🐛 デバッグ&ID実装ガイド（v3.2） |
| **LEARNING_MODE_GUIDE.md** | 🎛️ 学習モード切替ガイド（v3.1） |
| **IMPLEMENTATION_VERIFICATION.md** | 完全実装検証レポート |
| **TECHNICAL_DOCUMENT.md** | 完全技術資料（1,200行） |
| **PRESENTATION_SLIDES.md** | プレゼン用スライド（30枚） |
| **FINAL_REPORT.md** | 最終実装報告書 |
| **LEARNING_GUIDE.md** | 学習機能の詳細ガイド |
| **QUICKSTART.md** | クイックスタートガイド |
| **AI_IMPLEMENTATION.md** | AI実装の技術詳細 |

## ✨ 主要機能

- ✅ ビットボード
- ✅ Alpha-Beta探索
- ✅ 置換表（Zobrist Hashing）
- ✅ 序盤定石
- ✅ テンプレートマッチング評価
- ✅ TSS（詰み探索）
- ✅ 禁じ手判定
- ✅ CSV設定ファイル
- ✅ 学習機能
- ✅ 学習モード切替
- ✅ **デバッグ機能** 🆕
- ✅ **Iterative Deepening** 🆕

## 📊 学習の例

```
[Learning] Game finished! Result: WIN
[Learning] Total games: 12 (W:8 L:4 D:0)
[Learning] Win rate: 66.67%
[Learning] Pattern scores updated:
  OPEN_FOUR: 50250, FOUR: 10150, OPEN_THREE: 5180
[Learning] Learning data saved successfully.
```

## 🎓 学習の仕組み

1. **勝敗分析** - 各試合の結果を記録
2. **パターン評価** - 使用したパターンを分析
3. **スコア調整** - 勝率に応じて評価値を最適化
4. **定石学習** - 勝った手順を記憶
5. **データ保存** - 次回起動時に引き継ぎ

詳しくは **LEARNING_GUIDE.md** をご覧ください。

## 📁 プロジェクト構成

```
G-2025_group1/
├── gomoku_new.exe          # 実行ファイル
├── config.txt              # 設定（探索深度など）
├── patterns.csv            # パターンスコア
├── opening_book.csv        # 序盤定石
├── learning_data.dat       # 学習データ（自動生成）
│
├── gomoku_ai.c/h           # AIエントリポイント
├── config.c/h              # 設定管理
├── build.bat               # ビルドスクリプト
│
└── logics/
    ├── learning.c/h        # 学習システム 🆕
    ├── evaluation.c/h      # 評価関数
    ├── transposition.c/h   # 置換表
    ├── opening_book.c/h    # 序盤定石
    ├── main_engine.c/h     # Alpha-Beta探索
    └── TSS.c/h             # 詰み探索
```

## 🎮 使い方

### 基本的な対戦

```cmd
gomoku_new.exe
```

### 探索深度の変更

`config.txt` を編集：

```txt
SEARCH_DEPTH=4    # 1-2:初級, 3-4:中級, 5-6:上級, 7-8:大会レベル
```

### 🆕 デバッグレベルの設定

`config.txt` を編集：

```txt
# 詳細デバッグ出力（0=OFF, 1=基本, 2=詳細, 3=超詳細）
DEBUG_LEVEL=1
```

### ⏱️ 🆕 ID時間閾値の調整（v3.3）

`config.txt` を編集：

```txt
# Iterative Deepening 時間制限閾値（0.0～1.0）
# 制限時間の何%で次の深度に進まないか
ID_TIME_THRESHOLD=0.8    # 0.7:保守的, 0.8:推奨, 0.9:積極的
```

**重要**: 時間切れになった深度の結果は破棄され、前の深度の確実な結果を返します

**レベル別**:
- **0**: デバッグ出力なし（通常対戦）
- **1**: 基本情報（探索開始・終了、最善手）
- **2**: 詳細情報（探索統計、盤面表示）
- **3**: 超詳細（全候補手、枝刈り詳細）

### 🆕 Iterative Deepeningの設定

`config.txt` を編集：

```txt
# 反復深化探索（0=OFF, 1=ON）
ITERATIVE_DEEPENING=1
```

**メリット**:
- 時間制限への対応
- 探索効率の向上（置換表のヒット率アップ）
- 早期の詰み手発見

詳しくは **DEBUG_AND_ID_GUIDE.md** をご覧ください。

### 学習モードの切り替え

`config.txt` を編集：

```txt
# 練習モード（学習する）
LEARNING_MODE=PRACTICE

# 本番モード（学習しない、データのみ参照）
LEARNING_MODE=PRODUCTION
```

**使い分け**:
- **PRACTICE**: 日常の練習、AIのトレーニング期間
- **PRODUCTION**: 大会、本番対戦、性能評価

詳しくは **LEARNING_MODE_GUIDE.md** をご覧ください。

### 学習のリセット

```cmd
del learning_data.dat
```

## 📈 期待される効果

| 試合数 | 効果 |
|-------|------|
| 5-10試合 | 初歩的なミスが減少 |
| 20-50試合 | 勝率が安定、定石増加 |
| 100試合以上 | 理論上の最適値に収束 |

## 🏆 技術的特徴

- **C言語実装** - 高速動作
- **ビットボード** - 効率的な盤面表現
- **置換表** - 探索の高速化（2-5倍）
- **学習システム** - 自動的な強化

## 🔧 開発

### ビルド

```cmd
build.bat
```

### テスト

```cmd
# 学習機能のテスト
gcc test_learning.c logics/learning.c logics/evaluation.c config.c -o test_learning.exe
test_learning.exe
```

## 📝 ライセンス

MIT License

## 🎉 更新履歴

### v3.0 (2025-10-11)
- 🆕 学習機能の実装
- 🆕 パターンスコアの自動最適化
- 🆕 序盤定石の自動学習
- 🆕 学習データの永続化

### v2.0 (2025-10-11)
- ステップ4-5完成
- Zobrist Hashing + 置換表
- 序盤定石データベース
- CSV設定ファイル対応

### v1.0 (2025-10-11)
- 初版リリース
- ステップ1-3実装完了

## 🌟 今後の展開

- [ ] ニューラルネットワークの統合
- [ ] オンライン対戦機能
- [ ] GUI版の開発

---

**まずは対戦してAIを育ててください！** 🎮
