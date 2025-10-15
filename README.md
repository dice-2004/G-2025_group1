# 🎮 五目並べAI v3.4

高性能な五目並べAIプログラム。Alpha-Beta枝刈り、置換表、学習機能などの先進的なアルゴリズムを実装した本格的なAIシステムです。

## 📋 目次

- [🎯 プロジェクト概要](#-プロジェクト概要)
- [✨ 主要機能](#-主要機能)
- [🚀 クイックスタート](#-クイックスタート)
- [📚 ドキュメント](#-ドキュメント)
- [🗂️ プロジェクト構造](#️-プロジェクト構造)
- [🛠️ 開発環境](#️-開発環境)
- [📖 詳細ガイド](#-詳細ガイド)
- [📄 ライセンス](#-ライセンス)

---

## 🎯 プロジェクト概要

このプロジェクトは、C言語で実装された高性能な五目並べAIです。Alpha-Beta枝刈り、反復深化探索、置換表、学習機能など、コンピュータ将棋で使われる先進的な技術を採用しています。

### 特徴
- 🧠 **高度なAI**: Alpha-Beta枝刈り + 置換表による高速探索
- 📊 **学習機能**: ゲーム結果から学習し、評価関数を改善
- ⚡ **最適化**: 反復深化探索(ID)による時間管理
- 🎯 **評価関数**: 20種類以上のパターン認識による精密な局面評価
- 🔧 **カスタマイズ可能**: 設定ファイルで探索深さ、時間制限などを調整可能

---

## ✨ 主要機能

### 1. 強力なAI思考エンジン
- **Alpha-Beta枝刈り**: 不要な探索を削減し、効率的に最善手を探索
- **反復深化探索(ID)**: 時間内に可能な限り深く探索
- **置換表**: 同じ局面の再計算を避け、探索を高速化
- **開局定石**: 序盤の最善手をデータベース化

### 2. 学習システム
- **自動学習**: ゲーム結果から評価パラメータを自動調整
- **データ永続化**: 学習結果をファイルに保存し、次回起動時に復元
- **2モード対応**: 練習モード(PRACTICE)と本番モード(PRODUCTION)

### 3. 詳細なデバッグ機能
- **4段階のデバッグレベル**: ERROR, INFO, DEBUG, TRACEで詳細度を調整
- **探索ログ**: 探索の様子をリアルタイムで確認
- **統計情報**: ノード数、置換表ヒット率などの詳細統計

### 4. 柔軟な設定システム
- **config.txt**: 探索深さ、時間制限、デバッグレベルなどを設定
- **コマンドライン引数**: 一時的な設定変更も可能

---

## 🚀 クイックスタート

### インストール & 実行

#### Windows (build.bat)
```batch
build.bat
gomoku.exe
```

#### Linux/Mac (Makefile)
```bash
make
./gomoku.exe
```

### 基本的な使い方

1. **プログラムを起動**
   ```
   gomoku.exe
   ```

2. **手を入力** (例: `7 7` で中央に配置)
   ```
   あなたの手を入力してください (行 列): 7 7
   ```

3. **AIが応手を返す**

4. **ゲーム終了** (5つ並べたら勝利)

### 設定のカスタマイズ

`config.txt`を編集して、AIの動作をカスタマイズできます:

```
MAX_DEPTH=6              # 探索深さ
TIME_LIMIT_MS=5000       # 時間制限(ミリ秒)
DEBUG_LEVEL=1            # デバッグレベル(0-3)
LEARNING_MODE=PRODUCTION # 学習モード
```

**詳細は→** [クイックスタートガイド](./docs/quickstart/QUICKSTART.md)

---

## 📚 ドキュメント

プロジェクトの全ドキュメントは`docs`フォルダに整理されています。

### 📂 ドキュメント構成

```
docs/
├── quickstart/          クイックスタート・プレゼン資料
├── guides/              使い方・機能ガイド
├── technical/           技術ドキュメント
├── verification/        検証・最適化レポート
└── reports/             実装検証・報告書
```

### 🎯 目的別ナビゲーション

| 目的 | ドキュメント | 説明 |
|------|------------|------|
| **すぐに使い始めたい** | [クイックスタート](./docs/quickstart/QUICKSTART.md) | 5分で開始できるガイド |
| **全体像を把握したい** | [プレゼン用スライド](./docs/quickstart/PRESENTATION_SLIDES.md) | 30枚のプレゼン資料 |
| **システムを理解したい** | [システム動作ガイド](./docs/guides/SYSTEM_BEHAVIOR_GUIDE.md) | システムの詳細動作 |
| **技術詳細を知りたい** | [完全技術資料](./docs/technical/TECHNICAL_DOCUMENT.md) | ~1,200行の技術文書 |
| **設定方法を知りたい** | [使い方ガイド](./docs/guides/USAGE_GUIDE.md) | ビルド・実行・設定方法 |

---

## 📖 カテゴリ別ドキュメント詳細

### 📕 [クイックスタート・プレゼン](./docs/quickstart/)
**すぐに始めたい方、プロジェクトの概要を知りたい方向け**

| ファイル | 説明 | 所要時間 |
|---------|------|---------|
| [QUICKSTART.md](./docs/quickstart/QUICKSTART.md) | 🚀 クイックスタートガイド - 5分で始める | 5分 |
| [PRESENTATION_SLIDES.md](./docs/quickstart/PRESENTATION_SLIDES.md) | 📊 プレゼン用スライド - プロジェクト全体の概要 | 15分 |

**詳細は→** [クイックスタート README](./docs/quickstart/README.md)

---

### 📘 [ガイドドキュメント](./docs/guides/)
**システムの使い方、設定方法、機能の詳細ガイド**

#### プロジェクト概要ガイド
| ファイル | 説明 | 対象者 |
|---------|------|--------|
| [PROJECT_OVERVIEW.md](./docs/guides/PROJECT_OVERVIEW.md) | 🎯 プロジェクト概要 - 全体像、主要機能、技術的特徴 | 全員 |
| [USAGE_GUIDE.md](./docs/guides/USAGE_GUIDE.md) | 🚀 使い方ガイド - ビルド、実行、設定方法 | 全員 |
| [FEATURES.md](./docs/guides/FEATURES.md) | ✨ 機能詳細 - すべての機能の詳細説明 | 中級〜上級 |
| [PROJECT_STRUCTURE.md](./docs/guides/PROJECT_STRUCTURE.md) | 📁 プロジェクト構成ガイド - ディレクトリ構造 | 開発者 |

#### システム動作ガイド
| ファイル | 説明 | 対象者 |
|---------|------|--------|
| [SYSTEM_BEHAVIOR_GUIDE.md](./docs/guides/SYSTEM_BEHAVIOR_GUIDE.md) | 🔧 システム動作完全ガイド (v3.4) | 全員 |

#### 機能別ガイド
| ファイル | 説明 | 対象者 |
|---------|------|--------|
| [ID_TIME_MANAGEMENT.md](./docs/guides/ID_TIME_MANAGEMENT.md) | ⏱️ ID時間管理完全ガイド (v3.3) | 中級〜上級 |
| [DEBUG_AND_ID_GUIDE.md](./docs/guides/DEBUG_AND_ID_GUIDE.md) | 🐛 デバッグ&ID実装ガイド (v3.2) | 開発者 |
| [LEARNING_MODE_GUIDE.md](./docs/guides/LEARNING_MODE_GUIDE.md) | 🎛️ 学習モード切替ガイド (v3.1) | 全員 |

#### ビルドガイド
| ファイル | 説明 | 対象者 |
|---------|------|--------|
| [MAKEFILE_GUIDE.md](./docs/guides/MAKEFILE_GUIDE.md) | 🔨 Makefileガイド - ビルドシステムの詳細 | 開発者 |

**詳細は→** [ガイド README](./docs/guides/README.md)

---

### 📗 [技術ドキュメント](./docs/technical/)
**アルゴリズム、データ構造、実装の詳細解説**

#### 完全技術資料
| ファイル | 説明 | ページ数 |
|---------|------|---------|
| [TECHNICAL_DOCUMENT.md](./docs/technical/TECHNICAL_DOCUMENT.md) | 📖 完全技術資料 - 全アルゴリズム・データ構造の詳細 | ~1,200行 |

#### AI実装の詳細
| ファイル | 説明 | 対象者 |
|---------|------|--------|
| [AI_IMPLEMENTATION.md](./docs/technical/AI_IMPLEMENTATION.md) | 🤖 AI実装の技術詳細 - Alpha-Beta、置換表、評価関数 | 開発者 |

#### 評価関数の詳細
| ファイル | 説明 | 対象者 |
|---------|------|--------|
| [EVALUATION_PATTERNS.md](./docs/technical/EVALUATION_PATTERNS.md) | 🎯 評価パターンガイド - 20種類のパターン詳細 | 開発者・研究者 |

#### 学習システムの詳細
| ファイル | 説明 | 対象者 |
|---------|------|--------|
| [LEARNING_GUIDE.md](./docs/technical/LEARNING_GUIDE.md) | 📊 学習機能の詳細ガイド - 学習アルゴリズム | 開発者・研究者 |
| [LEARNING_IMPLEMENTATION.md](./docs/technical/LEARNING_IMPLEMENTATION.md) | 💡 学習システム実装詳細 - 実装手順と技術選択 | 開発者 |

**詳細は→** [技術ドキュメント README](./docs/technical/README.md)

---

### 📙 [検証・最適化レポート](./docs/verification/)
**評価関数の最適化、窓サイズの分析、正確性の検証**

#### 最新の検証レポート
| ファイル | 説明 | 日付 | 重要度 |
|---------|------|------|--------|
| [TIME_LIMIT_VERIFICATION.md](./docs/verification/TIME_LIMIT_VERIFICATION.md) | ⏱️ **時間制限機能の検証レポート** - 通常/厳格制限の動作検証 | 2025-10-15 | ⭐⭐⭐⭐⭐ |
| [TSS_AND_LEARNING_VERIFICATION.md](./docs/verification/TSS_AND_LEARNING_VERIFICATION.md) | 🎯 TSS機能と学習システム実装検証 | 2025-10-14 | ⭐⭐⭐⭐⭐ |

#### 最適化成果
| ファイル | 説明 | 日付 | 重要度 |
|---------|------|------|--------|
| [WINDOW_SIZE_ANALYSIS.md](./docs/verification/WINDOW_SIZE_ANALYSIS.md) | 🔍 窓サイズ詳細分析 | 2025-10-12 | ⭐⭐⭐⭐⭐ |
| [IMPLEMENTATION_REPORT_11x6.md](./docs/verification/IMPLEMENTATION_REPORT_11x6.md) | 📊 11マス抽出+6マス窓実装レポート | 2025-10-12 | ⭐⭐⭐⭐⭐ |

#### 正確性の検証
| ファイル | 説明 | 日付 | 重要度 |
|---------|------|------|--------|
| [TEMPLATE_MATCHING_ANALYSIS.md](./docs/verification/TEMPLATE_MATCHING_ANALYSIS.md) | ⚠️ テンプレートマッチング分析 | 2025-10-12 | ⭐⭐⭐⭐ |
| [RESTORATION_REPORT.md](./docs/verification/RESTORATION_REPORT.md) | 🔄 旧実装への復元レポート | 2025-10-12 | ⭐⭐⭐⭐ |

#### 最適化の実装と評価
| ファイル | 説明 | 日付 |
|---------|------|------|
| [OPTIMIZATION_COMPLETION.md](./docs/verification/OPTIMIZATION_COMPLETION.md) | ✅ 最適化完了レポート | 2025-10-12 |
| [EVALUATION_OPTIMIZATION.md](./docs/verification/EVALUATION_OPTIMIZATION.md) | 🚀 評価関数最適化ガイド | 2025-10-12 |

**詳細は→** [検証・最適化 README](./docs/verification/README.md)

---

### 📕 [実装検証・報告書](./docs/reports/)
**各バージョンの実装完了報告、検証レポート、修正レポート**

#### 実装検証レポート
| ファイル | 説明 |
|---------|------|
| [IMPLEMENTATION_VERIFICATION.md](./docs/reports/IMPLEMENTATION_VERIFICATION.md) | ✅ 完全実装検証レポート - 全機能の動作確認 |
| [IMPLEMENTATION_COMPLETE.md](./docs/reports/IMPLEMENTATION_COMPLETE.md) | 📝 実装完了報告 - 機能実装の詳細記録 |
| [FINAL_REPORT.md](./docs/reports/FINAL_REPORT.md) | 📋 最終実装報告書 - プロジェクト全体のまとめ |

#### バージョン別完了報告
| ファイル | 説明 | 実装日 |
|---------|------|--------|
| [COMPLETION_v3.3.md](./docs/reports/COMPLETION_v3.3.md) | 📝 v3.3完了報告 - ID時間管理機能 | 2025-10-11 |
| [COMPLETION_v3.2.md](./docs/reports/COMPLETION_v3.2.md) | 📝 v3.2完了報告 - デバッグ&ID機能 | 2025-10-11 |
| [COMPLETION_v3.1.md](./docs/reports/COMPLETION_v3.1.md) | 📝 v3.1完了報告 - 学習モード切替 | 2025-10-11 |
| [COMPLETION.md](./docs/reports/COMPLETION.md) | 📝 初期完了報告 - 基本機能 | 2025-10-11 |

#### 修正・メンテナンスレポート
| ファイル | 説明 | 実施日 |
|---------|------|--------|
| [MAKEFILE_FIX_REPORT.md](./docs/reports/MAKEFILE_FIX_REPORT.md) | 🔨 Makefile修正レポート - クロスプラットフォーム対応 | 2025-10-11 |
| [WARNING_FIX_REPORT.md](./docs/reports/WARNING_FIX_REPORT.md) | ⚠️ 警告修正レポート - コンパイラ警告の解消 | 2025-10-11 |
| [DOCUMENTATION_ORGANIZATION_REPORT.md](./docs/reports/DOCUMENTATION_ORGANIZATION_REPORT.md) | 📂 ドキュメント整理レポート | 2025-10-12 |
| [CATEGORY_ORGANIZATION_REPORT.md](./docs/reports/CATEGORY_ORGANIZATION_REPORT.md) | 🗂️ カテゴリ整理レポート | 2025-10-12 |

**詳細は→** [実装検証・報告書 README](./docs/reports/README.md)

---

## 📊 レベル別学習パス

### 🌱 初心者向け
1. [クイックスタート](./docs/quickstart/QUICKSTART.md) - まず使ってみる (5分)
2. [プロジェクト概要](./docs/guides/PROJECT_OVERVIEW.md) - プロジェクトを理解する (10分)
3. [システム動作ガイド](./docs/guides/SYSTEM_BEHAVIOR_GUIDE.md) - システムの動作を学ぶ (20分)
4. [学習モード切替ガイド](./docs/guides/LEARNING_MODE_GUIDE.md) - 学習機能を使う (10分)

### 🌿 中級者向け
1. [プレゼン用スライド](./docs/quickstart/PRESENTATION_SLIDES.md) - 全体像を把握する (15分)
2. [使い方ガイド](./docs/guides/USAGE_GUIDE.md) - 詳細な設定方法 (15分)
3. [AI実装の技術詳細](./docs/technical/AI_IMPLEMENTATION.md) - AIの仕組みを学ぶ (30分)
4. [デバッグ&ID実装ガイド](./docs/guides/DEBUG_AND_ID_GUIDE.md) - 高度な設定 (20分)

### 🌳 上級者・開発者向け
1. [完全技術資料](./docs/technical/TECHNICAL_DOCUMENT.md) - 実装の全詳細 (60分)
2. [窓サイズ詳細分析](./docs/verification/WINDOW_SIZE_ANALYSIS.md) - 最適化の詳細 (30分)
3. [実装検証レポート](./docs/reports/IMPLEMENTATION_VERIFICATION.md) - 動作確認手法 (20分)
4. [プロジェクト構成ガイド](./docs/guides/PROJECT_STRUCTURE.md) - コードベースの理解 (15分)

### 🔬 研究者向け
1. [完全技術資料](./docs/technical/TECHNICAL_DOCUMENT.md) - アルゴリズムの詳細 (60分)
2. [評価パターンガイド](./docs/technical/EVALUATION_PATTERNS.md) - 評価関数の詳細 (30分)
3. [検証・最適化レポート](./docs/verification/) - 最適化プロセス全体 (60分)
4. [学習機能の詳細ガイド](./docs/technical/LEARNING_GUIDE.md) - 学習アルゴリズム (40分)

---

## 🗂️ プロジェクト構造

```
G-2025_group1/
├── README.md                    ← このファイル
├── LICENSE                      ライセンス情報
├── Makefile                     ビルドシステム (Linux/Mac)
├── build.bat                    ビルドスクリプト (Windows)
│
├── gomoku.c                     メインプログラム
├── gomoku.h                     メインヘッダー
├── gomoku_ai.c/h                AI思考エンジン
├── config.c/h                   設定管理
│
├── logics/                      コアロジック
│   ├── main_engine.c/h          探索エンジン (Alpha-Beta)
│   ├── evaluation.c/h           評価関数
│   ├── learning.c/h             学習システム
│   ├── transposition.c/h        置換表
│   ├── opening_book.c/h         開局定石
│   ├── debug_utils.c/h          デバッグユーティリティ
│   └── TSS.c/h                  Threat Space Search
│
├── docs/                        📚 全ドキュメント
│   ├── README.md                ドキュメントのインデックス
│   ├── quickstart/              クイックスタート・プレゼン
│   ├── guides/                  ガイドドキュメント
│   ├── technical/               技術ドキュメント
│   ├── verification/            検証・最適化レポート
│   └── reports/                 実装検証・報告書
│
├── templates/                   設定ファイルのテンプレート
│   ├── example_config.txt       config.txtの例
│   ├── example_learning_output.txt  学習データの例
│   └── learning_data_format.txt 学習データフォーマット
│
├── config.txt                   設定ファイル
├── opening_book.csv             開局定石データ
├── patterns.csv                 評価パターンデータ
└── patterns_extended.csv        拡張評価パターンデータ
```

**詳細は→** [プロジェクト構成ガイド](./docs/guides/PROJECT_STRUCTURE.md)

---

## 🛠️ 開発環境

### 必要な環境
- **C言語コンパイラ**: GCC, Clang, MSVC など
- **OS**: Windows, Linux, macOS
- **メモリ**: 最低 512MB (推奨 1GB以上)

### ビルド方法

#### Windows
```batch
build.bat
```

#### Linux/Mac
```bash
make
```

#### カスタムビルド
```bash
# デバッグビルド
make debug

# クリーン
make clean

# 全体リビルド
make rebuild
```

**詳細は→** [使い方ガイド](./docs/guides/USAGE_GUIDE.md) | [Makefileガイド](./docs/guides/MAKEFILE_GUIDE.md)

---

## 📖 詳細ガイド

### よく使うドキュメント

#### 初めての方
- 📖 [クイックスタート](./docs/quickstart/QUICKSTART.md) - 5分で始める
- 📖 [プロジェクト概要](./docs/guides/PROJECT_OVERVIEW.md) - 全体像を把握

#### 使い方を知りたい
- 📖 [使い方ガイド](./docs/guides/USAGE_GUIDE.md) - ビルド・実行・設定
- 📖 [システム動作ガイド](./docs/guides/SYSTEM_BEHAVIOR_GUIDE.md) - システムの詳細動作
- 📖 [機能詳細](./docs/guides/FEATURES.md) - 全機能の説明

#### 技術的な詳細を知りたい
- 📖 [完全技術資料](./docs/technical/TECHNICAL_DOCUMENT.md) - ~1,200行の技術文書
- 📖 [AI実装の技術詳細](./docs/technical/AI_IMPLEMENTATION.md) - AIアルゴリズムの解説
- 📖 [評価パターンガイド](./docs/technical/EVALUATION_PATTERNS.md) - 評価関数の詳細

#### カスタマイズしたい
- 📖 [学習モード切替ガイド](./docs/guides/LEARNING_MODE_GUIDE.md) - 学習機能の設定
- 📖 [ID時間管理ガイド](./docs/guides/ID_TIME_MANAGEMENT.md) - 時間制限の調整
- 📖 [デバッグ&ID実装ガイド](./docs/guides/DEBUG_AND_ID_GUIDE.md) - デバッグ設定

#### 開発に参加したい
- 📖 [プロジェクト構成ガイド](./docs/guides/PROJECT_STRUCTURE.md) - コードベースの理解
- 📖 [Makefileガイド](./docs/guides/MAKEFILE_GUIDE.md) - ビルドシステム
- 📖 [実装検証レポート](./docs/reports/IMPLEMENTATION_VERIFICATION.md) - テスト手法

### すべてのドキュメント
📂 [docs/README.md](./docs/README.md) - すべてのドキュメントのインデックス

---

## 📊 ドキュメント統計

| カテゴリ | ドキュメント数 | 総ページ数（推定） |
|---------|--------------|------------------|
| クイックスタート・プレゼン | 2個 | ~100ページ |
| ガイドドキュメント | 9個 | ~300ページ |
| 技術ドキュメント | 5個 | ~1,500ページ |
| 検証・最適化レポート | 6個 | ~400ページ |
| 実装検証・報告書 | 11個 | ~600ページ |
| **合計** | **33個** | **~2,900ページ** |

---

## 🎯 バージョン履歴

| バージョン | 日付 | 主な変更内容 |
|-----------|------|-------------|
| v3.4 | 2025-10-12 | 最適化完了、窓サイズ分析 |
| v3.3 | 2025-10-11 | ID時間管理機能追加 |
| v3.2 | 2025-10-11 | デバッグ&ID機能強化 |
| v3.1 | 2025-10-11 | 学習モード切替機能 |
| v3.0 | 2025-10-11 | 基本機能完成 |

**詳細は→** [最終実装報告書](./docs/reports/FINAL_REPORT.md)

---

## 🤝 コントリビューション

プロジェクトへの貢献を歓迎します！

### 貢献方法
1. このリポジトリをフォーク
2. フィーチャーブランチを作成 (`git checkout -b feature/AmazingFeature`)
3. 変更をコミット (`git commit -m 'Add some AmazingFeature'`)
4. ブランチにプッシュ (`git push origin feature/AmazingFeature`)
5. プルリクエストを作成

### 開発ガイドライン
- コードスタイルを統一
- 適切なコメントを追加
- ドキュメントも更新

---

## 📄 ライセンス

このプロジェクトは [LICENSE](./LICENSE) ファイルに記載されたライセンスの下で公開されています。

---

## 📞 サポート・問い合わせ

- **Issues**: バグ報告や機能リクエストは [GitHub Issues](../../issues) へ
- **ドキュメント**: [docs/README.md](./docs/README.md) ですべてのドキュメントを確認

---

## 🙏 謝辞

このプロジェクトは、コンピュータ将棋の研究で培われた技術を五目並べに応用したものです。
アルゴリズムや実装に関する多くの研究・資料に感謝します。

---

**🎮 楽しい五目並べライフを！**

**📚 詳細なドキュメントは [docs/](./docs/) フォルダをご覧ください。**
