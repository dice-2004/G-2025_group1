# 五目並べAI v3.4 - 評価パターン拡張版# 五目並べAI v3.4 - 評価パターン拡張版# 五目並べAI v3.4 - 評価パターン拡張版



## 🎯 概要



**対戦するほど強くなる**五目並べAI実装です。## 🎯 概要## 🎯 概要



### 🆕 v3.4の新機能



🎯 **評価パターン大幅拡張** - より精密な局面評価**対戦するほど強くなる**五目並べAI実装です。**対戦するほど強くなる**五目並べAI実装です。

- **8パターン → 20パターン**に拡張

- 複合パターン対応（四四、四三、三々など）

- 飛びパターン検出（飛び四、飛び三、飛び二）

- 位置評価追加（中央、連結、敵石近接）### 🆕 v3.4の新機能### 🆕 v3.4の新機能

- **11マス抽出 + 6マス窓**の最適化実装 ✨



**何度も対戦してAIを育てよう！**

🎯 **評価パターン大幅拡張** - より精密な局面評価🎯 **評価パターン大幅拡張** - より精密な局面評価

## 🚀 クイックスタート

- **8パターン → 20パターン**に拡張- **8パターン → 20パターン**に拡張

### Windows (build.bat)

```cmd- 複合パターン対応（四四、四三、三々など）- 複合パターン対応（四四、四三、三々など）

# ビルド

build.bat- 飛びパターン検出（飛び四、飛び三、飛び二）- 飛びパターン検出（飛び四、飛び三、飛び二）



# 実行- 位置評価追加（中央、連結、敵石近接）- 位置評価追加（中央、連結、敵石近接）

gomoku_new.exe

```- **11マス抽出 + 6マス窓**の最適化実装 ✨- 詳細な評価による強さの向上



### Linux/Mac/MSYS2 (Makefile)

```bash

# ビルド### v3.3の機能### v3.3の機能

make all



# 実行

./gomoku⏱️ **ID時間管理機能** - Iterative Deepeningの時間制限を完全制御⏱️ **ID時間管理機能** - Iterative Deepeningの時間制限を完全制御



# クリーン- **時間切れ時に前の深度の結果を返す**（重要な改善！）- **時間切れ時に前の深度の結果を返す**（重要な改善！）

make clean

```- 時間閾値をconfig.txtで簡単に調整（0.7～0.95）- 時間閾値をconfig.txtで簡単に調整（0.7～0.95）



詳しくは → **[クイックスタートガイド](./docs/quickstart/QUICKSTART.md)**- 探索開始前と完了後の二段階チェック- 探索開始前と完了後の二段階チェック



---- 不完全な結果を使わず、確実な結果を返す



## 📚 ドキュメント### v3.2の機能



> 📂 **すべてのドキュメントは [docs/](./docs/) フォルダにカテゴリ別に整理されています。**### v3.2の機能



### 🗂️ ドキュメントカテゴリ🐛 **デバッグ機能** - 探索の詳細を可視化



| カテゴリ | 説明 | ドキュメント数 |- デバッグレベル 0-3（OFF/基本/詳細/超詳細）🐛 **デバッグ機能** - 探索の詳細を可視化

|---------|------|--------------|

| 📘 [**ガイドドキュメント**](./docs/guides/) | システムの使い方、設定方法、機能の詳細ガイド | 5個 |- 探索統計（ノード数、枝刈り率、TT hit率）- デバッグレベル 0-3（OFF/基本/詳細/超詳細）

| 📗 [**技術ドキュメント**](./docs/technical/) | アルゴリズム、データ構造、実装の詳細解説 | 4個 |

| 📙 [**検証・最適化レポート**](./docs/verification/) | 評価関数の最適化、窓サイズの分析、正確性の検証 | 6個 |- 探索統計（ノード数、枝刈り率、TT hit率）

| 📕 [**実装検証・報告書**](./docs/reports/) | 各バージョンの実装完了報告、検証レポート | 10個 |

| 📕 [**クイックスタート・プレゼン**](./docs/quickstart/) | すぐに始める方法、プロジェクト概要 | 2個 |🔄 **Iterative Deepening（反復深化探索）** - 時間制限への対応- 盤面表示、評価値詳細



**合計: 27個のドキュメント（~2,900ページ）**- 深さ1から徐々に探索



---- 時間制限内で最善手を返す🔄 **Iterative Deepening（反復深化探索）** - 時間制限への対応



### 🎯 目的別ナビゲーション- 深さ1から徐々に探索



<table>### v3.1の機能- 時間制限内で最善手を返す

<tr>

<td width="50%">- 置換表の効率向上



**🌱 初めての方**🎛️ **学習モード切替機能** - 練習モードと本番モードを選択可能- 早期の良手・詰み手発見

1. [クイックスタート](./docs/quickstart/QUICKSTART.md)

2. [システム動作ガイド](./docs/guides/SYSTEM_BEHAVIOR_GUIDE.md)- **練習モード（PRACTICE）**: 学習しながら成長

3. [学習モード切替](./docs/guides/LEARNING_MODE_GUIDE.md)

- **本番モード（PRODUCTION）**: 学習済みデータで安定動作### v3.1の機能

**🌿 中級者**

1. [プレゼンスライド](./docs/quickstart/PRESENTATION_SLIDES.md)

2. [AI実装詳細](./docs/technical/AI_IMPLEMENTATION.md)

3. [デバッグ&IDガイド](./docs/guides/DEBUG_AND_ID_GUIDE.md)### v3.0の機能🎛️ **学習モード切替機能** - 練習モードと本番モードを選択可能



</td>- **練習モード（PRACTICE）**: 学習しながら成長

<td width="50%">

✨ **学習機能** - 対戦を重ねるごとに自動的に強くなります- **本番モード（PRODUCTION）**: 学習済みデータで安定動作

**🌳 上級者・開発者**

1. [完全技術資料](./docs/technical/TECHNICAL_DOCUMENT.md)- パターンスコアの自動最適化

2. [窓サイズ分析](./docs/verification/WINDOW_SIZE_ANALYSIS.md)

3. [実装検証](./docs/reports/IMPLEMENTATION_VERIFICATION.md)- 序盤定石の自動学習### v3.0の機能



**🔬 研究者**- 学習データの永続化

1. [完全技術資料](./docs/technical/TECHNICAL_DOCUMENT.md)

2. [検証・最適化レポート](./docs/verification/)✨ **学習機能** - 対戦を重ねるごとに自動的に強くなります

3. [学習機能詳細](./docs/technical/LEARNING_GUIDE.md)

## 🚀 クイックスタート- パターンスコアの自動最適化

</td>

</tr>- 序盤定石の自動学習

</table>

### Windows (build.bat)- 学習データの永続化

---

```cmd

### 📖 主要ドキュメント（厳選）

# ビルド## 🚀 クイックスタート

#### 📘 ガイド

- [**システム動作完全ガイド**](./docs/guides/SYSTEM_BEHAVIOR_GUIDE.md) - 🆕 v3.4の全機能build.bat

- [**ID時間管理ガイド**](./docs/guides/ID_TIME_MANAGEMENT.md) - ⏱️ 時間制限の完全制御

- [**デバッグ&IDガイド**](./docs/guides/DEBUG_AND_ID_GUIDE.md) - 🐛 デバッグレベル0-3### Windows (build.bat)

- [**学習モード切替**](./docs/guides/LEARNING_MODE_GUIDE.md) - 🎛️ PRACTICE/PRODUCTION

# 実行```cmd

#### 📗 技術

- [**完全技術資料**](./docs/technical/TECHNICAL_DOCUMENT.md) - 📖 1,200行の詳細解説gomoku_new.exe# ビルド

- [**AI実装の技術詳細**](./docs/technical/AI_IMPLEMENTATION.md) - 🤖 Alpha-Beta、置換表

- [**学習機能詳細**](./docs/technical/LEARNING_GUIDE.md) - 📊 学習アルゴリズム```build.bat



#### 📙 検証・最適化（最新）

- [**窓サイズ詳細分析**](./docs/verification/WINDOW_SIZE_ANALYSIS.md) - 🔍 提案1/2/3の比較

- [**11マス抽出+6マス窓**](./docs/verification/IMPLEMENTATION_REPORT_11x6.md) - 📊 実装詳細### Linux/Mac/MSYS2 (Makefile)# 実行

- [**テンプレートマッチング分析**](./docs/verification/TEMPLATE_MATCHING_ANALYSIS.md) - ⚠️ 正確性検証

```bashgomoku_new.exe

すべてのドキュメントは → **[docs/README.md](./docs/README.md)** をご覧ください

# ビルド```

---

make all

## ✨ 主要機能

### Linux/Mac/MSYS2 (Makefile)

<table>

<tr># 実行```bash

<td width="50%">

./gomoku# ビルド

**探索・評価**

- ✅ ビットボードmake all

- ✅ Alpha-Beta探索

- ✅ 置換表（Zobrist Hashing）# クリーン

- ✅ 序盤定石

- ✅ 20種類の評価パターン 🆕make clean# 実行

- ✅ 11マス抽出 + 6マス窓 🆕

- ✅ TSS（詰み探索）```./gomoku

- ✅ 禁じ手判定



</td>

<td width="50%">**何度も対戦してAIを育てよう！**# クリーン



**学習・デバッグ**make clean

- ✅ 学習機能（パターンスコア調整）

- ✅ 学習モード切替## 📚 ドキュメント```

- ✅ CSV設定ファイル

- ✅ デバッグ機能（4段階レベル）

- ✅ Iterative Deepening

- ✅ 時間管理機能> 📂 **すべてのドキュメントは [docs/](./docs/) フォルダに格納されています。****何度も対戦してAIを育てよう！**

- ✅ 探索統計表示



</td>

</tr>### 📘 メインドキュメント## 📚 ドキュメント

</table>



---

| ファイル | 説明 |### 📘 メインドキュメント

## 📊 学習の例

|---------|------|

```

[Learning] Game finished! Result: WIN| [**SYSTEM_BEHAVIOR_GUIDE.md**](./docs/SYSTEM_BEHAVIOR_GUIDE.md) | 🆕 **システム動作完全ガイド（v3.4）** - learning.h詳細説明、動作フロー、ファイルI/O || ファイル | 説明 |

[Learning] Total games: 12 (W:8 L:4 D:0)

[Learning] Win rate: 66.67%| [**ID_TIME_MANAGEMENT.md**](./docs/ID_TIME_MANAGEMENT.md) | ⏱️ **ID時間管理完全ガイド（v3.3）** - 時間制限の完全制御、時間閾値調整 ||---------|------|

[Learning] Pattern scores updated:

  OPEN_FOUR: 50250, FOUR: 10150, OPEN_THREE: 5180| [**DEBUG_AND_ID_GUIDE.md**](./docs/DEBUG_AND_ID_GUIDE.md) | 🐛 **デバッグ&ID実装ガイド（v3.2）** - デバッグレベル、反復深化探索 || [**SYSTEM_BEHAVIOR_GUIDE.md**](./SYSTEM_BEHAVIOR_GUIDE.md) | 🆕 **システム動作完全ガイド（v3.4）** - learning.h詳細説明、動作フロー、ファイルI/O、画面表示、パターン拡張の全て |

[Learning] Learning data saved successfully.

```| [**LEARNING_MODE_GUIDE.md**](./docs/LEARNING_MODE_GUIDE.md) | 🎛️ **学習モード切替ガイド（v3.1）** - PRACTICE/PRODUCTION切替 || [**TEMPLATE_MATCHING_ANALYSIS.md**](./TEMPLATE_MATCHING_ANALYSIS.md) | ⚠️ 🆕 **テンプレートマッチング分析** - 最適化試行の詳細分析と正確性の検証 |



## 🎓 学習の仕組み| [**MAKEFILE_GUIDE.md**](./docs/MAKEFILE_GUIDE.md) | 🔧 **Makefile完全ガイド** - Linux/Mac/MSYS2対応ビルド || [**RESTORATION_REPORT.md**](./RESTORATION_REPORT.md) | � 🆕 **旧実装への復元レポート** - 正確性問題の発見と復元の経緯 |



1. **勝敗分析** - 各試合の結果を記録| [**ID_TIME_MANAGEMENT.md**](./ID_TIME_MANAGEMENT.md) | ⏱️ **ID時間管理完全ガイド（v3.3）** - 時間制限の完全制御、時間閾値調整 |

2. **パターン評価** - 使用したパターンを分析

3. **スコア調整** - 勝率に応じて評価値を最適化### 📗 技術ドキュメント| [**DEBUG_AND_ID_GUIDE.md**](./DEBUG_AND_ID_GUIDE.md) | 🐛 **デバッグ&ID実装ガイド（v3.2）** - デバッグレベル、反復深化探索 |

4. **定石学習** - 勝った手順を記憶

5. **データ保存** - 次回起動時に引き継ぎ| [**LEARNING_MODE_GUIDE.md**](./LEARNING_MODE_GUIDE.md) | 🎛️ **学習モード切替ガイド（v3.1）** - PRACTICE/PRODUCTION切替 |



詳しくは → **[学習機能詳細ガイド](./docs/technical/LEARNING_GUIDE.md)**| ファイル | 説明 || [**MAKEFILE_GUIDE.md**](./MAKEFILE_GUIDE.md) | 🔧 **Makefile完全ガイド** - Linux/Mac/MSYS2対応ビルド |



---|---------|------|



## 📁 プロジェクト構成| [**TECHNICAL_DOCUMENT.md**](./docs/TECHNICAL_DOCUMENT.md) | 📖 **完全技術資料（1,200行）** - 全アルゴリズム・データ構造の詳細 |### 📗 技術ドキュメント



```| [**AI_IMPLEMENTATION.md**](./docs/AI_IMPLEMENTATION.md) | 🤖 **AI実装の技術詳細** - Alpha-Beta、置換表、評価関数 |

G-2025_group1/

├── README.md                  ← このファイル| [**LEARNING_GUIDE.md**](./docs/LEARNING_GUIDE.md) | 📊 **学習機能の詳細ガイド** - 学習アルゴリズム、データ永続化 || ファイル | 説明 |

│

├── docs/                      ← 📚 全ドキュメント（27個）| [**LEARNING_IMPLEMENTATION.md**](./docs/LEARNING_IMPLEMENTATION.md) | 💡 **学習システム実装詳細** - 実装手順と技術選択 ||---------|------|

│   ├── README.md              ← ドキュメントトップ

│   ├── guides/                ← 📘 ガイドドキュメント（5個）| [**TECHNICAL_DOCUMENT.md**](./TECHNICAL_DOCUMENT.md) | 📖 **完全技術資料（1,200行）** - 全アルゴリズム・データ構造の詳細 |

│   ├── technical/             ← 📗 技術ドキュメント（4個）

│   ├── verification/          ← 📙 検証・最適化レポート（6個）### 📙 検証・最適化レポート（最新）| [**AI_IMPLEMENTATION.md**](./AI_IMPLEMENTATION.md) | 🤖 **AI実装の技術詳細** - Alpha-Beta、置換表、評価関数 |

│   ├── reports/               ← 📕 実装検証・報告書（10個）

│   └── quickstart/            ← 📕 クイックスタート・プレゼン（2個）| [**LEARNING_GUIDE.md**](./LEARNING_GUIDE.md) | 📊 **学習機能の詳細ガイド** - 学習アルゴリズム、データ永続化 |

│

├── templates/                 ← テンプレートファイル| ファイル | 説明 || [**LEARNING_IMPLEMENTATION.md**](./LEARNING_IMPLEMENTATION.md) | 💡 **学習システム実装詳細** - 実装手順と技術選択 |

│   ├── example_config.txt

│   ├── example_learning_output.txt|---------|------|

│   └── learning_data_format.txt

│| [**WINDOW_SIZE_ANALYSIS.md**](./docs/WINDOW_SIZE_ANALYSIS.md) | 🔍 **窓サイズ詳細分析** - 抽出と窓の違い、提案1/2/3の具体的比較、処理フロー |### 📙 実装検証・報告書

├── logics/                    ← AI思考エンジン

│   ├── evaluation.c/h         ← 🆕 評価関数（20パターン、11x6窓）| [**IMPLEMENTATION_REPORT_11x6.md**](./docs/IMPLEMENTATION_REPORT_11x6.md) | 📊 **11マス抽出+6マス窓実装レポート** - 実装詳細、利点、性能影響分析 |

│   ├── main_engine.c/h        ← Alpha-Beta、ID実装

│   ├── TSS.c/h                ← 詰み探索| [**TEMPLATE_MATCHING_ANALYSIS.md**](./docs/TEMPLATE_MATCHING_ANALYSIS.md) | ⚠️ **テンプレートマッチング分析** - 最適化試行の詳細分析と正確性の検証 || ファイル | 説明 |

│   ├── transposition.c/h      ← 置換表

│   ├── opening_book.c/h       ← 序盤定石| [**RESTORATION_REPORT.md**](./docs/RESTORATION_REPORT.md) | 🔄 **旧実装への復元レポート** - 正確性問題の発見と復元の経緯 ||---------|------|

│   ├── learning.c/h           ← 学習システム

│   └── debug_utils.c/h        ← デバッグユーティリティ| [**OPTIMIZATION_COMPLETION.md**](./docs/OPTIMIZATION_COMPLETION.md) | ✅ **最適化完了レポート** - 評価関数最適化の実装と検証 || [**IMPLEMENTATION_VERIFICATION.md**](./IMPLEMENTATION_VERIFICATION.md) | ✅ **完全実装検証レポート** - 全機能の動作確認 |

│

├── gomoku.c/h                 ← ゲームインターフェース| [**EVALUATION_OPTIMIZATION.md**](./docs/EVALUATION_OPTIMIZATION.md) | 🚀 **評価関数最適化ガイド** - 最適化手法と実装方法 || [**FINAL_REPORT.md**](./FINAL_REPORT.md) | 📋 **最終実装報告書** - プロジェクト全体のまとめ |

├── gomoku_ai.c/h              ← AIエントリポイント

├── config.c/h                 ← 設定ファイル管理| [**COMPLETION_v3.3.md**](./COMPLETION_v3.3.md) | 📝 **v3.3完了報告** - ID時間管理機能の実装完了 |

│

├── config.txt                 ← AI設定ファイル### 📕 実装検証・報告書| [**COMPLETION_v3.2.md**](./COMPLETION_v3.2.md) | 📝 **v3.2完了報告** - デバッグ&ID機能の実装完了 |

├── patterns.csv               ← 基本パターンスコア

├── patterns_extended.csv      ← 🆕 拡張パターン（20個）| [**COMPLETION_v3.1.md**](./COMPLETION_v3.1.md) | 📝 **v3.1完了報告** - 学習モード切替の実装完了 |

├── opening_book.csv           ← 序盤定石データベース

│| ファイル | 説明 || [**COMPLETION.md**](./COMPLETION.md) | 📝 **初期完了報告** - 基本機能の実装完了 |

├── build.bat                  ← Windows用ビルドスクリプト

├── Makefile                   ← Linux/Mac/MSYS2用|---------|------|| [**MAKEFILE_FIX_REPORT.md**](./MAKEFILE_FIX_REPORT.md) | 🔨 **Makefile修正レポート** - クロスプラットフォーム対応 |

│

├── gomoku_new.exe             ← Windows用実行ファイル| [**IMPLEMENTATION_VERIFICATION.md**](./docs/IMPLEMENTATION_VERIFICATION.md) | ✅ **完全実装検証レポート** - 全機能の動作確認 || [**WARNING_FIX_REPORT.md**](./WARNING_FIX_REPORT.md) | ⚠️ **警告修正レポート** - コンパイラ警告の解消 |

├── gomoku.exe                 ← 元のゲーム

│| [**IMPLEMENTATION_COMPLETE.md**](./docs/IMPLEMENTATION_COMPLETE.md) | 📝 **実装完了報告** - 機能実装の詳細記録 |

├── LICENSE                    ← MITライセンス

└── gomoku.pdf                 ← 元の仕様書| [**FINAL_REPORT.md**](./docs/FINAL_REPORT.md) | 📋 **最終実装報告書** - プロジェクト全体のまとめ |### 📕 クイックスタート

```

| [**COMPLETION_v3.3.md**](./docs/COMPLETION_v3.3.md) | 📝 **v3.3完了報告** - ID時間管理機能の実装完了 |

---

| [**COMPLETION_v3.2.md**](./docs/COMPLETION_v3.2.md) | 📝 **v3.2完了報告** - デバッグ&ID機能の実装完了 || ファイル | 説明 |

## 🎮 使い方

| [**COMPLETION_v3.1.md**](./docs/COMPLETION_v3.1.md) | 📝 **v3.1完了報告** - 学習モード切替の実装完了 ||---------|------|

### 基本的な対戦

| [**COMPLETION.md**](./docs/COMPLETION.md) | 📝 **初期完了報告** - 基本機能の実装完了 || [**QUICKSTART.md**](./QUICKSTART.md) | 🚀 **クイックスタートガイド** - 5分で始める五目並べAI |

```cmd

gomoku_new.exe| [**PRESENTATION_SLIDES.md**](./PRESENTATION_SLIDES.md) | 📊 **プレゼン用スライド（30枚）** - プロジェクト全体の概要 |

```

### 📗 修正・メンテナンスレポート

### 探索深度の変更

## ✨ 主要機能

`config.txt` を編集：

| ファイル | 説明 |

```txt

SEARCH_DEPTH=4    # 1-2:初級, 3-4:中級, 5-6:上級, 7-8:大会レベル|---------|------|- ✅ ビットボード

```

| [**MAKEFILE_FIX_REPORT.md**](./docs/MAKEFILE_FIX_REPORT.md) | 🔨 **Makefile修正レポート** - クロスプラットフォーム対応 |- ✅ Alpha-Beta探索

### デバッグレベルの設定

| [**WARNING_FIX_REPORT.md**](./docs/WARNING_FIX_REPORT.md) | ⚠️ **警告修正レポート** - コンパイラ警告の解消 |- ✅ 置換表（Zobrist Hashing）

```txt

DEBUG_LEVEL=1     # 0=OFF, 1=基本, 2=詳細, 3=超詳細- ✅ 序盤定石

```

### 📕 クイックスタート・プレゼン- ✅ **20種類の評価パターン**（複合・飛び・位置評価） 🆕

### 学習モードの切り替え

- ✅ テンプレートマッチング評価

```txt

LEARNING_MODE=PRACTICE      # 学習しながら成長| ファイル | 説明 |- ✅ TSS（詰み探索）

# または

LEARNING_MODE=PRODUCTION    # 学習済みデータで安定動作|---------|------|- ✅ 禁じ手判定

```

| [**QUICKSTART.md**](./docs/QUICKSTART.md) | 🚀 **クイックスタートガイド** - 5分で始める五目並べAI |- ✅ CSV設定ファイル

### ID時間閾値の調整

| [**PRESENTATION_SLIDES.md**](./docs/PRESENTATION_SLIDES.md) | 📊 **プレゼン用スライド（30枚）** - プロジェクト全体の概要 |- ✅ 学習機能（パターンスコア自動調整）

```txt

ID_TIME_THRESHOLD=0.8    # 0.7:保守的, 0.8:推奨, 0.9:積極的- ✅ 学習モード切替（PRACTICE/PRODUCTION）

```

## ✨ 主要機能- ✅ デバッグ機能（4段階レベル）

詳しい設定方法は → **[ガイドドキュメント](./docs/guides/)**

- ✅ Iterative Deepening（時間管理付き）

---

- ✅ ビットボード

## 📈 期待される効果

- ✅ Alpha-Beta探索## 📊 学習の例

| 試合数 | 効果 |

|-------|------|- ✅ 置換表（Zobrist Hashing）

| 5-10試合 | 初歩的なミスが減少 |

| 20-50試合 | 勝率が安定、定石増加 |- ✅ 序盤定石```

| 100試合以上 | 理論上の最適値に収束 |

- ✅ **20種類の評価パターン**（複合・飛び・位置評価） 🆕[Learning] Game finished! Result: WIN

---

- ✅ **11マス抽出 + 6マス窓**による精密な評価 🆕[Learning] Total games: 12 (W:8 L:4 D:0)

## 🏆 技術的特徴

- ✅ テンプレートマッチング評価[Learning] Win rate: 66.67%

- **C言語実装** - 高速動作

- **ビットボード** - 効率的な盤面表現- ✅ TSS（詰み探索）[Learning] Pattern scores updated:

- **置換表** - 探索の高速化（2-5倍）

- **11マス抽出 + 6マス窓** - 精密な評価（開いた四を正確に検出）- ✅ 禁じ手判定  OPEN_FOUR: 50250, FOUR: 10150, OPEN_THREE: 5180

- **学習システム** - 自動的な強化

- **Iterative Deepening** - 時間制限への対応- ✅ CSV設定ファイル[Learning] Learning data saved successfully.

- **デバッグ機能** - 4段階レベル

- ✅ 学習機能（パターンスコア自動調整）```

---

- ✅ 学習モード切替（PRACTICE/PRODUCTION）

## 🔧 開発

- ✅ デバッグ機能（4段階レベル）## 🎓 学習の仕組み

### ビルド

- ✅ Iterative Deepening（時間管理付き）

**Windows:**

```cmd1. **勝敗分析** - 各試合の結果を記録

build.bat

```## 📊 学習の例2. **パターン評価** - 使用したパターンを分析



**Linux/Mac/MSYS2:**3. **スコア調整** - 勝率に応じて評価値を最適化

```bash

make all```4. **定石学習** - 勝った手順を記憶

```

[Learning] Game finished! Result: WIN5. **データ保存** - 次回起動時に引き継ぎ

### テスト

[Learning] Total games: 12 (W:8 L:4 D:0)

```cmd

gcc test_learning.c logics/learning.c logics/evaluation.c config.c -o test_learning.exe[Learning] Win rate: 66.67%詳しくは **LEARNING_GUIDE.md** をご覧ください。

test_learning.exe

```[Learning] Pattern scores updated:



詳しくは → **[Makefileガイド](./docs/guides/MAKEFILE_GUIDE.md)**  OPEN_FOUR: 50250, FOUR: 10150, OPEN_THREE: 5180## 📁 プロジェクト構成



---[Learning] Learning data saved successfully.



## 📝 ライセンス```### 🎮 実行ファイル・ビルドスクリプト



MIT License



---## 🎓 学習の仕組み| ファイル | 説明 |



## 🎉 更新履歴|---------|------|



### v3.4 (2025-10-12)1. **勝敗分析** - 各試合の結果を記録| [**gomoku_new.exe**](./gomoku_new.exe) | Windows用実行ファイル（学習機能付き） |

- 🆕 **評価パターン大幅拡張**（8 → 20パターン）

- 🆕 複合パターン対応（四四、四三、三々など）2. **パターン評価** - 使用したパターンを分析| [**gomoku.exe**](./gomoku.exe) | 元のゲームインターフェース |

- 🆕 飛びパターン検出（飛び四、飛び三、飛び二）

- 🆕 位置評価追加（中央、連結、敵石近接）3. **スコア調整** - 勝率に応じて評価値を最適化| [**build.bat**](./build.bat) | Windows用ビルドスクリプト |

- 🆕 **11マス抽出 + 6マス窓**の実装

  - 開いた四を正確に検出可能4. **定石学習** - 勝った手順を記憶| [**Makefile**](./Makefile) | Linux/Mac/MSYS2用ビルドファイル |

  - 両側の状況を確認可能

  - スライディングウィンドウで多角的評価5. **データ保存** - 次回起動時に引き継ぎ

- 🆕 **ドキュメント大幅整理**

  - カテゴリ別フォルダ構成### ⚙️ 設定・データファイル

  - 27個のドキュメント（~2,900ページ）

  - 目的別ナビゲーション詳しくは **[LEARNING_GUIDE.md](./docs/LEARNING_GUIDE.md)** をご覧ください。

- ⚠️ **評価関数最適化の試行と復元**

  - 最適化を試行（85.8%削減）したが、正確性問題を発見| ファイル | 説明 |

  - 窓サイズの最適化により精度向上を実現

## 📁 プロジェクト構成|---------|------|

### v3.3 (2025-10-11)

- 🆕 ID時間管理機能| [**config.txt**](./config.txt) | AI設定ファイル（探索深度、学習モード、デバッグレベルなど） |

- 🆕 時間閾値調整（config.txt）

### 🎮 実行ファイル・ビルドスクリプト| [**patterns.csv**](./patterns.csv) | 基本パターンスコア（8パターン） |

### v3.2 (2025-10-11)

- 🆕 デバッグ機能（レベル0-3）| [**patterns_extended.csv**](./patterns_extended.csv) | 🆕 拡張パターンスコア（20パターン、日本語説明付き） |

- 🆕 Iterative Deepening

| ファイル | 説明 || [**opening_book.csv**](./opening_book.csv) | 序盤定石データベース |

### v3.1 (2025-10-11)

- 🆕 学習モード切替（PRACTICE/PRODUCTION）|---------|------|| **learning_data.dat** | 学習データ（自動生成・バイナリ） |



### v3.0 (2025-10-11)| **gomoku_new.exe** | Windows用実行ファイル（学習機能付き） || [**pos.txt**](./pos.txt) | テスト用局面データ |

- 🆕 学習機能の実装

| **gomoku.exe** | 元のゲームインターフェース |

詳しくは → **[実装検証・報告書](./docs/reports/)**

| **build.bat** | Windows用ビルドスクリプト |### 📂 templates/ - テンプレートファイル

---

| **Makefile** | Linux/Mac/MSYS2用ビルドファイル |

## 🌟 今後の展開

| ファイル | 説明 |

- [ ] ニューラルネットワークの統合

- [ ] オンライン対戦機能### ⚙️ 設定・データファイル|---------|------|

- [ ] GUI版の開発

- [ ] より高度な評価関数（パターン自動検出）| [**templates/example_config.txt**](./templates/example_config.txt) | 🆕 設定ファイルテンプレート（詳細説明・設定例付き） |

- [ ] 並列探索

| ファイル | 説明 || [**templates/example_learning_output.txt**](./templates/example_learning_output.txt) | 🆕 学習モード実行例（画面出力サンプル） |

---

|---------|------|| [**templates/learning_data_format.txt**](./templates/learning_data_format.txt) | 🆕 learning_data.datフォーマット説明（バイナリ構造） |

## 📞 サポート

| **config.txt** | AI設定ファイル（探索深度、学習モード、デバッグレベルなど） |

- 📖 **ドキュメント**: [docs/](./docs/)

- 🚀 **クイックスタート**: [docs/quickstart/QUICKSTART.md](./docs/quickstart/QUICKSTART.md)| **patterns.csv** | 基本パターンスコア（8パターン） |### 💻 ソースコード（メインモジュール）

- 📊 **プレゼン**: [docs/quickstart/PRESENTATION_SLIDES.md](./docs/quickstart/PRESENTATION_SLIDES.md)

- 🐛 **Issue**: GitHubのissueを立ててください| **patterns_extended.csv** | 🆕 拡張パターンスコア（20パターン、日本語説明付き） |



---| **opening_book.csv** | 序盤定石データベース || ファイル | 説明 |



**まずは対戦してAIを育ててください！** 🎮| **learning_data.dat** | 学習データ（自動生成・バイナリ） ||---------|------|



質問・問題があれば、[ドキュメント](./docs/)を参照するか、issueを立ててください。| **pos.txt** | テスト用局面データ || [**gomoku.c**](./gomoku.c) / [**gomoku.h**](./gomoku.h) | ゲームインターフェース（元のファイル、非改変） |


| [**gomoku_ai.c**](./gomoku_ai.c) / [**gomoku_ai.h**](./gomoku_ai.h) | AIエントリポイント（initAI, AI, finishAI） |

### 📂 docs/ - ドキュメント| [**config.c**](./config.c) / [**config.h**](./config.h) | 設定ファイル読み込み管理 |



すべての技術ドキュメント、検証レポート、ガイドが格納されています。### 📂 logics/ - AI思考エンジン

- 📘 メインドキュメント

- 📗 技術ドキュメント#### 🧠 探索・評価

- 📙 検証・最適化レポート

- 📕 実装検証・報告書| ファイル | 説明 |

- 📗 修正・メンテナンスレポート|---------|------|

- 📕 クイックスタート・プレゼン| [**logics/main_engine.c**](./logics/main_engine.c) / [**.h**](./logics/main_engine.h) | Alpha-Beta探索、Iterative Deepening実装 |

| [**logics/evaluation.c**](./logics/evaluation.c) / [**.h**](./logics/evaluation.h) | 🆕 評価関数（20パターン対応） |

### 📂 templates/ - テンプレートファイル| [**logics/TSS.c**](./logics/TSS.c) / [**.h**](./logics/TSS.h) | 詰み探索（Threat Space Search） |



| ファイル | 説明 |#### 💾 データ構造・最適化

|---------|------|

| **templates/example_config.txt** | 🆕 設定ファイルテンプレート（詳細説明・設定例付き） || ファイル | 説明 |

| **templates/example_learning_output.txt** | 🆕 学習モード実行例（画面出力サンプル） ||---------|------|

| **templates/learning_data_format.txt** | 🆕 learning_data.datフォーマット説明（バイナリ構造） || [**logics/transposition.c**](./logics/transposition.c) / [**.h**](./logics/transposition.h) | 置換表（Zobrist Hashing） |

| [**logics/opening_book.c**](./logics/opening_book.c) / [**.h**](./logics/opening_book.h) | 序盤定石データベース |

### 💻 ソースコード（メインモジュール）

#### 🎓 学習システム

| ファイル | 説明 |

|---------|------|| ファイル | 説明 |

| **gomoku.c** / **gomoku.h** | ゲームインターフェース（元のファイル、非改変） ||---------|------|

| **gomoku_ai.c** / **gomoku_ai.h** | AIエントリポイント（initAI, AI, finishAI） || [**logics/learning.c**](./logics/learning.c) / [**.h**](./logics/learning.h) | 学習システム（対戦記録、スコア調整、データ永続化） |

| **config.c** / **config.h** | 設定ファイル読み込み管理 |

#### 🐛 デバッグ

### 📂 logics/ - AI思考エンジン

| ファイル | 説明 |

#### 🧠 探索・評価|---------|------|

| [**logics/debug_utils.c**](./logics/debug_utils.c) / [**.h**](./logics/debug_utils.h) | デバッグ出力ユーティリティ（4段階レベル） |

| ファイル | 説明 |

|---------|------|### 🧪 テスト

| **logics/main_engine.c** / **.h** | Alpha-Beta探索、Iterative Deepening実装 |

| **logics/evaluation.c** / **.h** | 🆕 評価関数（20パターン対応、11マス抽出+6マス窓） || ファイル | 説明 |

| **logics/TSS.c** / **.h** | 詰み探索（Threat Space Search） ||---------|------|

| [**test_learning.c**](./test_learning.c) | 学習機能単体テスト |

#### 💾 データ構造・最適化

### 📜 その他

| ファイル | 説明 |

|---------|------|| ファイル | 説明 |

| **logics/transposition.c** / **.h** | 置換表（Zobrist Hashing） ||---------|------|

| **logics/opening_book.c** / **.h** | 序盤定石データベース || [**LICENSE**](./LICENSE) | MITライセンス |

| [**gomoku.pdf**](./gomoku.pdf) | 元の仕様書 |

#### 🎓 学習システム| [**.gitignore**](./.gitignore) | Git除外設定 |



| ファイル | 説明 |## 🎮 使い方

|---------|------|

| **logics/learning.c** / **.h** | 学習システム（対戦記録、スコア調整、データ永続化） |### 基本的な対戦



#### 🐛 デバッグ```cmd

gomoku_new.exe

| ファイル | 説明 |```

|---------|------|

| **logics/debug_utils.c** / **.h** | デバッグ出力ユーティリティ（4段階レベル） |### 探索深度の変更



### 🧪 テスト`config.txt` を編集：



| ファイル | 説明 |```txt

|---------|------|SEARCH_DEPTH=4    # 1-2:初級, 3-4:中級, 5-6:上級, 7-8:大会レベル

| **test_learning.c** | 学習機能単体テスト |```



### 📜 その他### 🆕 デバッグレベルの設定



| ファイル | 説明 |`config.txt` を編集：

|---------|------|

| **LICENSE** | MITライセンス |```txt

| **gomoku.pdf** | 元の仕様書 |# 詳細デバッグ出力（0=OFF, 1=基本, 2=詳細, 3=超詳細）

| **.gitignore** | Git除外設定 |DEBUG_LEVEL=1

```

## 🎮 使い方

### ⏱️ 🆕 ID時間閾値の調整（v3.3）

### 基本的な対戦

`config.txt` を編集：

```cmd

gomoku_new.exe```txt

```# Iterative Deepening 時間制限閾値（0.0～1.0）

# 制限時間の何%で次の深度に進まないか

### 探索深度の変更ID_TIME_THRESHOLD=0.8    # 0.7:保守的, 0.8:推奨, 0.9:積極的

```

`config.txt` を編集：

**重要**: 時間切れになった深度の結果は破棄され、前の深度の確実な結果を返します

```txt

SEARCH_DEPTH=4    # 1-2:初級, 3-4:中級, 5-6:上級, 7-8:大会レベル**レベル別**:

```- **0**: デバッグ出力なし（通常対戦）

- **1**: 基本情報（探索開始・終了、最善手）

### 🆕 デバッグレベルの設定- **2**: 詳細情報（探索統計、盤面表示）

- **3**: 超詳細（全候補手、枝刈り詳細）

`config.txt` を編集：

### 🆕 Iterative Deepeningの設定

```txt

# 詳細デバッグ出力（0=OFF, 1=基本, 2=詳細, 3=超詳細）`config.txt` を編集：

DEBUG_LEVEL=1

``````txt

# 反復深化探索（0=OFF, 1=ON）

**レベル別**:ITERATIVE_DEEPENING=1

- **0**: デバッグ出力なし（通常対戦）```

- **1**: 基本情報（探索開始・終了、最善手）

- **2**: 詳細情報（探索統計、盤面表示）**メリット**:

- **3**: 超詳細（全候補手、枝刈り詳細）- 時間制限への対応

- 探索効率の向上（置換表のヒット率アップ）

### ⏱️ 🆕 ID時間閾値の調整（v3.3）- 早期の詰み手発見



`config.txt` を編集：詳しくは **DEBUG_AND_ID_GUIDE.md** をご覧ください。



```txt### 学習モードの切り替え

# Iterative Deepening 時間制限閾値（0.0～1.0）

# 制限時間の何%で次の深度に進まないか`config.txt` を編集：

ID_TIME_THRESHOLD=0.8    # 0.7:保守的, 0.8:推奨, 0.9:積極的

``````txt

# 練習モード（学習する）

**重要**: 時間切れになった深度の結果は破棄され、前の深度の確実な結果を返しますLEARNING_MODE=PRACTICE



### 🆕 Iterative Deepeningの設定# 本番モード（学習しない、データのみ参照）

LEARNING_MODE=PRODUCTION

`config.txt` を編集：```



```txt**使い分け**:

# 反復深化探索（0=OFF, 1=ON）- **PRACTICE**: 日常の練習、AIのトレーニング期間

ITERATIVE_DEEPENING=1- **PRODUCTION**: 大会、本番対戦、性能評価

```

詳しくは **LEARNING_MODE_GUIDE.md** をご覧ください。

**メリット**:

- 時間制限への対応### 学習のリセット

- 探索効率の向上（置換表のヒット率アップ）

- 早期の詰み手発見```cmd

del learning_data.dat

詳しくは **[DEBUG_AND_ID_GUIDE.md](./docs/DEBUG_AND_ID_GUIDE.md)** をご覧ください。```



### 学習モードの切り替え## 📈 期待される効果



`config.txt` を編集：| 試合数 | 効果 |

|-------|------|

```txt| 5-10試合 | 初歩的なミスが減少 |

# 練習モード（学習する）| 20-50試合 | 勝率が安定、定石増加 |

LEARNING_MODE=PRACTICE| 100試合以上 | 理論上の最適値に収束 |



# 本番モード（学習しない、データのみ参照）## 🏆 技術的特徴

LEARNING_MODE=PRODUCTION

```- **C言語実装** - 高速動作

- **ビットボード** - 効率的な盤面表現

**使い分け**:- **置換表** - 探索の高速化（2-5倍）

- **PRACTICE**: 日常の練習、AIのトレーニング期間- **学習システム** - 自動的な強化

- **PRODUCTION**: 大会、本番対戦、性能評価

## 🔧 開発

詳しくは **[LEARNING_MODE_GUIDE.md](./docs/LEARNING_MODE_GUIDE.md)** をご覧ください。

### ビルド

### 学習のリセット

```cmd

```cmdbuild.bat

del learning_data.dat```

```

### テスト

## 📈 期待される効果

```cmd

| 試合数 | 効果 |# 学習機能のテスト

|-------|------|gcc test_learning.c logics/learning.c logics/evaluation.c config.c -o test_learning.exe

| 5-10試合 | 初歩的なミスが減少 |test_learning.exe

| 20-50試合 | 勝率が安定、定石増加 |```

| 100試合以上 | 理論上の最適値に収束 |

## 📝 ライセンス

## 🏆 技術的特徴

MIT License

- **C言語実装** - 高速動作

- **ビットボード** - 効率的な盤面表現## 🎉 更新履歴

- **置換表** - 探索の高速化（2-5倍）

- **11マス抽出 + 6マス窓** - 精密な評価（開いた四を正確に検出）### v3.4 (2025-10-12)

- **学習システム** - 自動的な強化- 🆕 **評価パターン大幅拡張**（8 → 20パターン）

- 🆕 複合パターン対応（四四、四三、三々など）

## 🔧 開発- 🆕 飛びパターン検出（飛び四、飛び三、飛び二）

- 🆕 位置評価追加（中央、連結、敵石近接）

### ビルド- 🆕 システム動作完全ガイド作成

- 🆕 テンプレートファイル作成（config、output、data format）

```cmd- ⚠️ **評価関数最適化の試行と復元**

build.bat  - 最適化を試行（85.8%削減）したが、重大な正確性問題を発見

```  - スコアの重複カウント問題（最大57.8%の誤差）

  - 複合パターン検出不足（四四、四三など）

### テスト  - 詳細な分析により旧実装に復元（正確性を優先）

  - 今後はキャッシングや増分評価で最適化を検討

```cmd

# 学習機能のテスト### v3.3 (2025-10-11)

gcc test_learning.c logics/learning.c logics/evaluation.c config.c -o test_learning.exe- 🆕 ID時間管理機能（時間切れ時に前の深度の結果を返す）

test_learning.exe- 🆕 時間閾値をconfig.txtで調整可能（ID_TIME_THRESHOLD）

```- 🆕 探索開始前と完了後の二段階チェック



## 📝 ライセンス### v3.2 (2025-10-11)

- 🆕 デバッグ機能（DEBUG_LEVEL 0-3）

MIT License- 🆕 Iterative Deepening（反復深化探索）

- 🆕 探索統計表示（ノード数、枝刈り率、TT hit率）

## 🎉 更新履歴- 🆕 debug_utils モジュール追加



### v3.4 (2025-10-12)### v3.1 (2025-10-11)

- 🆕 **評価パターン大幅拡張**（8 → 20パターン）- 🆕 学習モード切替（PRACTICE/PRODUCTION）

- 🆕 複合パターン対応（四四、四三、三々など）- 🆕 config.txtでモード設定

- 🆕 飛びパターン検出（飛び四、飛び三、飛び二）- 🆕 本番環境での安定動作

- 🆕 位置評価追加（中央、連結、敵石近接）

- 🆕 **11マス抽出 + 6マス窓**の実装### v3.0 (2025-10-11)

  - 開いた四を正確に検出可能- 🆕 学習機能の実装

  - 両側の状況を確認可能- 🆕 パターンスコアの自動最適化

  - スライディングウィンドウで多角的評価- 🆕 序盤定石の自動学習

- 🆕 システム動作完全ガイド作成- 🆕 学習データの永続化（learning_data.dat）

- 🆕 テンプレートファイル作成（config、output、data format）

- ⚠️ **評価関数最適化の試行と復元**### v2.0 (2025-10-11)

  - 最適化を試行（85.8%削減）したが、重大な正確性問題を発見- ステップ4-5完成

  - スコアの重複カウント問題（最大57.8%の誤差）- Zobrist Hashing + 置換表

  - 複合パターン検出不足（四四、四三など）- 序盤定石データベース

  - 詳細な分析により旧実装に復元（正確性を優先）- CSV設定ファイル対応

  - 窓サイズの最適化により精度向上を実現

### v1.0 (2025-10-11)

### v3.3 (2025-10-11)- 初版リリース

- 🆕 ID時間管理機能（時間切れ時に前の深度の結果を返す）- ステップ1-3実装完了（ビットボード、Alpha-Beta、評価関数）

- 🆕 時間閾値をconfig.txtで調整可能（ID_TIME_THRESHOLD）

- 🆕 探索開始前と完了後の二段階チェック---



### v3.2 (2025-10-11)## 📖 さらに詳しく

- 🆕 デバッグ機能（DEBUG_LEVEL 0-3）

- 🆕 Iterative Deepening（反復深化探索）各ファイルの詳細は、上記のリンクから該当ファイルを参照してください。

- 🆕 探索統計表示（ノード数、枝刈り率、TT hit率）

- 🆕 debug_utils モジュール追加### 🎯 おすすめの読み方



### v3.1 (2025-10-11)1. **初めての方**: [QUICKSTART.md](./QUICKSTART.md) → [SYSTEM_BEHAVIOR_GUIDE.md](./SYSTEM_BEHAVIOR_GUIDE.md)

- 🆕 学習モード切替（PRACTICE/PRODUCTION）2. **技術詳細を知りたい方**: [TECHNICAL_DOCUMENT.md](./TECHNICAL_DOCUMENT.md) → [AI_IMPLEMENTATION.md](./AI_IMPLEMENTATION.md)

- 🆕 config.txtでモード設定3. **学習機能について**: [LEARNING_GUIDE.md](./LEARNING_GUIDE.md) → [LEARNING_MODE_GUIDE.md](./LEARNING_MODE_GUIDE.md)

- 🆕 本番環境での安定動作4. **実装を確認したい方**: [IMPLEMENTATION_VERIFICATION.md](./IMPLEMENTATION_VERIFICATION.md) → 各完了報告書

5. **ビルド方法**: [MAKEFILE_GUIDE.md](./MAKEFILE_GUIDE.md)

### v3.0 (2025-10-11)

- 🆕 学習機能の実装## 🌟 今後の展開

- 🆕 パターンスコアの自動最適化

- 🆕 序盤定石の自動学習- [ ] ニューラルネットワークの統合

- 🆕 学習データの永続化（learning_data.dat）- [ ] オンライン対戦機能

- [ ] GUI版の開発

### v2.0 (2025-10-11)- [ ] より高度な評価関数（パターン自動検出）

- ステップ4-5完成- [ ] 並列探索

- Zobrist Hashing + 置換表

- 序盤定石データベース---

- CSV設定ファイル対応

**まずは対戦してAIを育ててください！** 🎮

### v1.0 (2025-10-11)

- 初版リリース質問・問題があれば、各ドキュメントを参照するか、issueを立ててください。

- ステップ1-3実装完了（ビットボード、Alpha-Beta、評価関数）

---

## 📖 さらに詳しく

各ファイルの詳細は、[docs/](./docs/) フォルダ内のドキュメントを参照してください。

### 🎯 おすすめの読み方

1. **初めての方**: [QUICKSTART.md](./docs/QUICKSTART.md) → [SYSTEM_BEHAVIOR_GUIDE.md](./docs/SYSTEM_BEHAVIOR_GUIDE.md)
2. **技術詳細を知りたい方**: [TECHNICAL_DOCUMENT.md](./docs/TECHNICAL_DOCUMENT.md) → [AI_IMPLEMENTATION.md](./docs/AI_IMPLEMENTATION.md)
3. **学習機能について**: [LEARNING_GUIDE.md](./docs/LEARNING_GUIDE.md) → [LEARNING_MODE_GUIDE.md](./docs/LEARNING_MODE_GUIDE.md)
4. **最適化の詳細**: [WINDOW_SIZE_ANALYSIS.md](./docs/WINDOW_SIZE_ANALYSIS.md) → [IMPLEMENTATION_REPORT_11x6.md](./docs/IMPLEMENTATION_REPORT_11x6.md)
5. **実装を確認したい方**: [IMPLEMENTATION_VERIFICATION.md](./docs/IMPLEMENTATION_VERIFICATION.md) → 各完了報告書
6. **ビルド方法**: [MAKEFILE_GUIDE.md](./docs/MAKEFILE_GUIDE.md)

## 🌟 今後の展開

- [ ] ニューラルネットワークの統合
- [ ] オンライン対戦機能
- [ ] GUI版の開発
- [ ] より高度な評価関数（パターン自動検出）
- [ ] 並列探索

---

**まずは対戦してAIを育ててください！** 🎮

質問・問題があれば、各ドキュメントを参照するか、issueを立ててください。
