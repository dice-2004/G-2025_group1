# 📚 ドキュメント

五目並べAI v3.4の全ドキュメントを集約したフォルダです。カテゴリ別に整理されています。

## 🗂️ カテゴリ一覧

### 📘 [ガイドドキュメント](./guides/)
システムの使い方、設定方法、機能の詳細ガイド

**主なドキュメント:**
- システム動作完全ガイド
- ID時間管理ガイド
- デバッグ&ID実装ガイド
- 学習モード切替ガイド
- Makefileガイド

[→ ガイドドキュメントを見る](./guides/)

---

### 📗 [技術ドキュメント](./technical/)
アルゴリズム、データ構造、実装の詳細解説

**主なドキュメント:**
- 完全技術資料（~1,200行）
- AI実装の技術詳細
- 学習機能の詳細ガイド
- 学習システム実装詳細

[→ 技術ドキュメントを見る](./technical/)

---

### 📙 [検証・最適化レポート](./verification/)
評価関数の最適化、窓サイズの分析、正確性の検証

**主なドキュメント:**
- 🆕 **Opening Book実装総まとめ** - 論理的問題の修正完了
- 🆕 **Opening Book修正完了レポート** - 11種類の定石追加
- 🆕 **Opening Book論理的検証** - 3つの重大な問題の発見
- 🆕 **Opening Bookゲーム単位選択の実装** - 手順整合性保証
- 🆕 窓サイズ詳細分析
- 🆕 11マス抽出+6マス窓実装レポート
- テンプレートマッチング分析
- 旧実装への復元レポート
- 最適化完了レポート

[→ 検証・最適化レポートを見る](./verification/)

---

### 📕 [実装検証・報告書](./reports/)
各バージョンの実装完了報告、検証レポート、修正レポート

**主なドキュメント:**
- 完全実装検証レポート
- 最終実装報告書
- バージョン別完了報告（v3.1, v3.2, v3.3）
- Makefile修正レポート
- 警告修正レポート
- ドキュメント整理レポート

[→ 実装検証・報告書を見る](./reports/)

---

### 📕 [クイックスタート・プレゼン](./quickstart/)
すぐに始めたい方、プロジェクトの概要を知りたい方向け

**主なドキュメント:**
- クイックスタートガイド（5分で開始）
- プレゼン用スライド（30枚）

[→ クイックスタート・プレゼンを見る](./quickstart/)

---

## 🎯 目的別ナビゲーション

### すぐに使い始めたい
→ [クイックスタート](./quickstart/QUICKSTART.md)

### システム全体を理解したい
→ [システム動作完全ガイド](./guides/SYSTEM_BEHAVIOR_GUIDE.md)

### 技術詳細を知りたい
→ [完全技術資料](./technical/TECHNICAL_DOCUMENT.md)

### 戦略理論を理解したい（NEW!）
→ [防御戦略の分析](./technical/DEFENSE_STRATEGY_ANALYSIS.md)

### Opening Bookの修正内容を知りたい（NEW!）
→ [Opening Book実装総まとめ](./verification/OPENING_BOOK_CORRECTION_SUMMARY.md)

### 最適化の詳細を知りたい
→ [窓サイズ詳細分析](./verification/WINDOW_SIZE_ANALYSIS.md)

### プロジェクトの全体像を把握したい
→ [プレゼン用スライド](./quickstart/PRESENTATION_SLIDES.md)

---

## 📖 読む順番（レベル別）

### 🌱 初心者向け
1. [クイックスタート](./quickstart/QUICKSTART.md) - まず使ってみる
2. [システム動作完全ガイド](./guides/SYSTEM_BEHAVIOR_GUIDE.md) - システムの理解
3. [学習モード切替ガイド](./guides/LEARNING_MODE_GUIDE.md) - 学習モードの活用

### 🌿 中級者向け
1. [プレゼン用スライド](./quickstart/PRESENTATION_SLIDES.md) - 全体像の把握
2. [AI実装の技術詳細](./technical/AI_IMPLEMENTATION.md) - AI実装の理解
3. [デバッグ&ID実装ガイド](./guides/DEBUG_AND_ID_GUIDE.md) - 詳細設定

### 🌳 上級者・開発者向け
1. [完全技術資料](./technical/TECHNICAL_DOCUMENT.md) - 実装の詳細
2. [窓サイズ詳細分析](./verification/WINDOW_SIZE_ANALYSIS.md) - 最適化の詳細
3. [実装検証レポート](./reports/IMPLEMENTATION_VERIFICATION.md) - 動作確認

### 🔬 研究者向け
1. [完全技術資料](./technical/TECHNICAL_DOCUMENT.md) - アルゴリズムの詳細
2. [検証・最適化レポート](./verification/) - 最適化プロセス全体
3. [学習機能の詳細ガイド](./technical/LEARNING_GUIDE.md) - 学習アルゴリズム

---

## 📊 ドキュメント統計

| カテゴリ | ドキュメント数 | 総ページ数（推定） |
|---------|--------------|------------------|
| クイックスタート・プレゼン | 3個 | ~100ページ |
| ガイドドキュメント | 10個 | ~300ページ |
| 技術ドキュメント | 7個 | ~1,500ページ |
| 検証・最適化レポート | 15個 | ~700ページ |
| 実装検証・報告書 | 15個 | ~700ページ |
| **合計** | **50個** | **~3,300ページ** |

---

## 🔍 検索のヒント

### 特定の機能を調べたい
- **学習機能**: [学習機能の詳細ガイド](./technical/LEARNING_GUIDE.md)
- **デバッグ**: [デバッグ&ID実装ガイド](./guides/DEBUG_AND_ID_GUIDE.md)
- **時間管理**: [ID時間管理ガイド](./guides/ID_TIME_MANAGEMENT.md)
- **評価関数**: [完全技術資料](./technical/TECHNICAL_DOCUMENT.md) の評価関数セクション

### Opening Book関連を調べたい（NEW!）
- **実装総まとめ**: [Opening Book実装総まとめ](./verification/OPENING_BOOK_CORRECTION_SUMMARY.md)
- **修正完了レポート**: [Opening Book修正完了レポート](./verification/OPENING_BOOK_CORRECTION_REPORT.md)
- **論理的検証**: [Opening Book論理的検証](./verification/OPENING_BOOK_LOGICAL_VERIFICATION.md)
- **ゲーム単位選択**: [Opening Bookゲーム単位選択の実装](./verification/OPENING_BOOK_GAME_SEQUENCE_IMPLEMENTATION.md)

### 特定のバージョンを調べたい
- **v3.3**: [COMPLETION_v3.3.md](./reports/COMPLETION_v3.3.md)
- **v3.2**: [COMPLETION_v3.2.md](./reports/COMPLETION_v3.2.md)
- **v3.1**: [COMPLETION_v3.1.md](./reports/COMPLETION_v3.1.md)

### 最適化の詳細を調べたい
- **窓サイズ**: [窓サイズ詳細分析](./verification/WINDOW_SIZE_ANALYSIS.md)
- **実装**: [11マス抽出+6マス窓実装レポート](./verification/IMPLEMENTATION_REPORT_11x6.md)
- **正確性**: [テンプレートマッチング分析](./verification/TEMPLATE_MATCHING_ANALYSIS.md)

---

## 🏠 戻る

- [プロジェクトルート](../) に戻る

---

## 📝 ドキュメントの追加・更新

### 新しいドキュメントを追加する場合
1. 適切なカテゴリフォルダに配置
2. そのフォルダの `README.md` に追加
3. 必要に応じてこのファイルも更新

### ドキュメントを更新する場合
1. 該当ファイルを直接編集
2. 大きな変更の場合は各カテゴリの `README.md` も更新

---

**ドキュメントに関する質問・提案があれば、issueを立ててください。**
