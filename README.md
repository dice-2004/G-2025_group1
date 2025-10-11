# 五目並べAI v3.4 - 評価パターン拡張版

## 🎯 概要

**対戦するほど強くなる**五目並べAI実装です。

### 🆕 v3.4の新機能

🎯 **評価パターン大幅拡張** - より精密な局面評価
- **8パターン → 20パターン**に拡張
- 複合パターン対応（四四、四三、三々など）
- 飛びパターン検出（飛び四、飛び三、飛び二）
- 位置評価追加（中央、連結、敵石近接）
- 詳細な評価による強さの向上

### v3.3の機能

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

### 📘 メインドキュメント

| ファイル | 説明 |
|---------|------|
| [**SYSTEM_BEHAVIOR_GUIDE.md**](./SYSTEM_BEHAVIOR_GUIDE.md) | 🆕 **システム動作完全ガイド（v3.4）** - learning.h詳細説明、動作フロー、ファイルI/O、画面表示、パターン拡張の全て |
| [**ID_TIME_MANAGEMENT.md**](./ID_TIME_MANAGEMENT.md) | ⏱️ **ID時間管理完全ガイド（v3.3）** - 時間制限の完全制御、時間閾値調整 |
| [**DEBUG_AND_ID_GUIDE.md**](./DEBUG_AND_ID_GUIDE.md) | 🐛 **デバッグ&ID実装ガイド（v3.2）** - デバッグレベル、反復深化探索 |
| [**LEARNING_MODE_GUIDE.md**](./LEARNING_MODE_GUIDE.md) | 🎛️ **学習モード切替ガイド（v3.1）** - PRACTICE/PRODUCTION切替 |
| [**MAKEFILE_GUIDE.md**](./MAKEFILE_GUIDE.md) | 🔧 **Makefile完全ガイド** - Linux/Mac/MSYS2対応ビルド |

### 📗 技術ドキュメント

| ファイル | 説明 |
|---------|------|
| [**TECHNICAL_DOCUMENT.md**](./TECHNICAL_DOCUMENT.md) | 📖 **完全技術資料（1,200行）** - 全アルゴリズム・データ構造の詳細 |
| [**AI_IMPLEMENTATION.md**](./AI_IMPLEMENTATION.md) | 🤖 **AI実装の技術詳細** - Alpha-Beta、置換表、評価関数 |
| [**LEARNING_GUIDE.md**](./LEARNING_GUIDE.md) | 📊 **学習機能の詳細ガイド** - 学習アルゴリズム、データ永続化 |
| [**LEARNING_IMPLEMENTATION.md**](./LEARNING_IMPLEMENTATION.md) | 💡 **学習システム実装詳細** - 実装手順と技術選択 |

### 📙 実装検証・報告書

| ファイル | 説明 |
|---------|------|
| [**IMPLEMENTATION_VERIFICATION.md**](./IMPLEMENTATION_VERIFICATION.md) | ✅ **完全実装検証レポート** - 全機能の動作確認 |
| [**FINAL_REPORT.md**](./FINAL_REPORT.md) | 📋 **最終実装報告書** - プロジェクト全体のまとめ |
| [**COMPLETION_v3.3.md**](./COMPLETION_v3.3.md) | 📝 **v3.3完了報告** - ID時間管理機能の実装完了 |
| [**COMPLETION_v3.2.md**](./COMPLETION_v3.2.md) | 📝 **v3.2完了報告** - デバッグ&ID機能の実装完了 |
| [**COMPLETION_v3.1.md**](./COMPLETION_v3.1.md) | 📝 **v3.1完了報告** - 学習モード切替の実装完了 |
| [**COMPLETION.md**](./COMPLETION.md) | 📝 **初期完了報告** - 基本機能の実装完了 |
| [**MAKEFILE_FIX_REPORT.md**](./MAKEFILE_FIX_REPORT.md) | 🔨 **Makefile修正レポート** - クロスプラットフォーム対応 |
| [**WARNING_FIX_REPORT.md**](./WARNING_FIX_REPORT.md) | ⚠️ **警告修正レポート** - コンパイラ警告の解消 |

### 📕 クイックスタート

| ファイル | 説明 |
|---------|------|
| [**QUICKSTART.md**](./QUICKSTART.md) | 🚀 **クイックスタートガイド** - 5分で始める五目並べAI |
| [**PRESENTATION_SLIDES.md**](./PRESENTATION_SLIDES.md) | 📊 **プレゼン用スライド（30枚）** - プロジェクト全体の概要 |

## ✨ 主要機能

- ✅ ビットボード
- ✅ Alpha-Beta探索
- ✅ 置換表（Zobrist Hashing）
- ✅ 序盤定石
- ✅ **20種類の評価パターン**（複合・飛び・位置評価） 🆕
- ✅ テンプレートマッチング評価
- ✅ TSS（詰み探索）
- ✅ 禁じ手判定
- ✅ CSV設定ファイル
- ✅ 学習機能（パターンスコア自動調整）
- ✅ 学習モード切替（PRACTICE/PRODUCTION）
- ✅ デバッグ機能（4段階レベル）
- ✅ Iterative Deepening（時間管理付き）

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

### 🎮 実行ファイル・ビルドスクリプト

| ファイル | 説明 |
|---------|------|
| [**gomoku_new.exe**](./gomoku_new.exe) | Windows用実行ファイル（学習機能付き） |
| [**gomoku.exe**](./gomoku.exe) | 元のゲームインターフェース |
| [**build.bat**](./build.bat) | Windows用ビルドスクリプト |
| [**Makefile**](./Makefile) | Linux/Mac/MSYS2用ビルドファイル |

### ⚙️ 設定・データファイル

| ファイル | 説明 |
|---------|------|
| [**config.txt**](./config.txt) | AI設定ファイル（探索深度、学習モード、デバッグレベルなど） |
| [**patterns.csv**](./patterns.csv) | 基本パターンスコア（8パターン） |
| [**patterns_extended.csv**](./patterns_extended.csv) | 🆕 拡張パターンスコア（20パターン、日本語説明付き） |
| [**opening_book.csv**](./opening_book.csv) | 序盤定石データベース |
| **learning_data.dat** | 学習データ（自動生成・バイナリ） |
| [**pos.txt**](./pos.txt) | テスト用局面データ |

### 📂 templates/ - テンプレートファイル

| ファイル | 説明 |
|---------|------|
| [**templates/example_config.txt**](./templates/example_config.txt) | 🆕 設定ファイルテンプレート（詳細説明・設定例付き） |
| [**templates/example_learning_output.txt**](./templates/example_learning_output.txt) | 🆕 学習モード実行例（画面出力サンプル） |
| [**templates/learning_data_format.txt**](./templates/learning_data_format.txt) | 🆕 learning_data.datフォーマット説明（バイナリ構造） |

### 💻 ソースコード（メインモジュール）

| ファイル | 説明 |
|---------|------|
| [**gomoku.c**](./gomoku.c) / [**gomoku.h**](./gomoku.h) | ゲームインターフェース（元のファイル、非改変） |
| [**gomoku_ai.c**](./gomoku_ai.c) / [**gomoku_ai.h**](./gomoku_ai.h) | AIエントリポイント（initAI, AI, finishAI） |
| [**config.c**](./config.c) / [**config.h**](./config.h) | 設定ファイル読み込み管理 |

### 📂 logics/ - AI思考エンジン

#### 🧠 探索・評価

| ファイル | 説明 |
|---------|------|
| [**logics/main_engine.c**](./logics/main_engine.c) / [**.h**](./logics/main_engine.h) | Alpha-Beta探索、Iterative Deepening実装 |
| [**logics/evaluation.c**](./logics/evaluation.c) / [**.h**](./logics/evaluation.h) | 🆕 評価関数（20パターン対応） |
| [**logics/TSS.c**](./logics/TSS.c) / [**.h**](./logics/TSS.h) | 詰み探索（Threat Space Search） |

#### 💾 データ構造・最適化

| ファイル | 説明 |
|---------|------|
| [**logics/transposition.c**](./logics/transposition.c) / [**.h**](./logics/transposition.h) | 置換表（Zobrist Hashing） |
| [**logics/opening_book.c**](./logics/opening_book.c) / [**.h**](./logics/opening_book.h) | 序盤定石データベース |

#### 🎓 学習システム

| ファイル | 説明 |
|---------|------|
| [**logics/learning.c**](./logics/learning.c) / [**.h**](./logics/learning.h) | 学習システム（対戦記録、スコア調整、データ永続化） |

#### 🐛 デバッグ

| ファイル | 説明 |
|---------|------|
| [**logics/debug_utils.c**](./logics/debug_utils.c) / [**.h**](./logics/debug_utils.h) | デバッグ出力ユーティリティ（4段階レベル） |

### 🧪 テスト

| ファイル | 説明 |
|---------|------|
| [**test_learning.c**](./test_learning.c) | 学習機能単体テスト |

### 📜 その他

| ファイル | 説明 |
|---------|------|
| [**LICENSE**](./LICENSE) | MITライセンス |
| [**gomoku.pdf**](./gomoku.pdf) | 元の仕様書 |
| [**.gitignore**](./.gitignore) | Git除外設定 |

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

### v3.4 (2025-10-11)
- 🆕 **評価パターン大幅拡張**（8 → 20パターン）
- 🆕 複合パターン対応（四四、四三、三々など）
- 🆕 飛びパターン検出（飛び四、飛び三、飛び二）
- 🆕 位置評価追加（中央、連結、敵石近接）
- 🆕 システム動作完全ガイド作成
- 🆕 テンプレートファイル作成（config、output、data format）

### v3.3 (2025-10-11)
- 🆕 ID時間管理機能（時間切れ時に前の深度の結果を返す）
- 🆕 時間閾値をconfig.txtで調整可能（ID_TIME_THRESHOLD）
- 🆕 探索開始前と完了後の二段階チェック

### v3.2 (2025-10-11)
- 🆕 デバッグ機能（DEBUG_LEVEL 0-3）
- 🆕 Iterative Deepening（反復深化探索）
- 🆕 探索統計表示（ノード数、枝刈り率、TT hit率）
- 🆕 debug_utils モジュール追加

### v3.1 (2025-10-11)
- 🆕 学習モード切替（PRACTICE/PRODUCTION）
- 🆕 config.txtでモード設定
- 🆕 本番環境での安定動作

### v3.0 (2025-10-11)
- 🆕 学習機能の実装
- 🆕 パターンスコアの自動最適化
- 🆕 序盤定石の自動学習
- 🆕 学習データの永続化（learning_data.dat）

### v2.0 (2025-10-11)
- ステップ4-5完成
- Zobrist Hashing + 置換表
- 序盤定石データベース
- CSV設定ファイル対応

### v1.0 (2025-10-11)
- 初版リリース
- ステップ1-3実装完了（ビットボード、Alpha-Beta、評価関数）

---

## 📖 さらに詳しく

各ファイルの詳細は、上記のリンクから該当ファイルを参照してください。

### 🎯 おすすめの読み方

1. **初めての方**: [QUICKSTART.md](./QUICKSTART.md) → [SYSTEM_BEHAVIOR_GUIDE.md](./SYSTEM_BEHAVIOR_GUIDE.md)
2. **技術詳細を知りたい方**: [TECHNICAL_DOCUMENT.md](./TECHNICAL_DOCUMENT.md) → [AI_IMPLEMENTATION.md](./AI_IMPLEMENTATION.md)
3. **学習機能について**: [LEARNING_GUIDE.md](./LEARNING_GUIDE.md) → [LEARNING_MODE_GUIDE.md](./LEARNING_MODE_GUIDE.md)
4. **実装を確認したい方**: [IMPLEMENTATION_VERIFICATION.md](./IMPLEMENTATION_VERIFICATION.md) → 各完了報告書
5. **ビルド方法**: [MAKEFILE_GUIDE.md](./MAKEFILE_GUIDE.md)

## 🌟 今後の展開

- [ ] ニューラルネットワークの統合
- [ ] オンライン対戦機能
- [ ] GUI版の開発
- [ ] より高度な評価関数（パターン自動検出）
- [ ] 並列探索

---

**まずは対戦してAIを育ててください！** 🎮

質問・問題があれば、各ドキュメントを参照するか、issueを立ててください。
