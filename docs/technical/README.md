# 📗 技術ドキュメント

アルゴリズム、データ構造、実装の詳細を解説した技術資料を集約したフォルダです。

## 📚 ドキュメント一覧

### 完全技術資料

| ファイル | 説明 | ページ数 | 対象者 |
|---------|------|---------|--------|
| [**TECHNICAL_DOCUMENT.md**](./TECHNICAL_DOCUMENT.md) | 📖 **完全技術資料** - 全アルゴリズム・データ構造の詳細 | ~1,200行 | 開発者・研究者 |

### AI実装の詳細

| ファイル | 説明 | 対象者 |
|---------|------|--------|
| [**AI_IMPLEMENTATION.md**](./AI_IMPLEMENTATION.md) | 🤖 **AI実装の技術詳細** - Alpha-Beta、置換表、評価関数 | 開発者 |

### 評価関数の詳細（🆕）

| ファイル | 説明 | 対象者 |
|---------|------|--------|
| [**EVALUATION_PATTERNS.md**](./EVALUATION_PATTERNS.md) | 🎯 **評価パターンガイド** - 20種類のパターン詳細、カスタマイズ方法 | 開発者・研究者 |

### 戦略分析（🆕）

| ファイル | 説明 | 対象者 |
|---------|------|--------|
| [**DEFENSE_STRATEGY_ANALYSIS.md**](./DEFENSE_STRATEGY_ANALYSIS.md) | 🛡️ **防御戦略の分析** - 開いた三防御の必要性、戦略理論、勝率分析 | 開発者・研究者 |
| [**DEFENSE_STRATEGY_SUPPLEMENT.md**](./DEFENSE_STRATEGY_SUPPLEMENT.md) | ⚡ **防御戦略の補足** - 実戦例に基づく問題点の発見と修正提案 | 開発者 |

### 学習システムの詳細

| ファイル | 説明 | 対象者 |
|---------|------|--------|
| [**LEARNING_GUIDE.md**](./LEARNING_GUIDE.md) | 📊 **学習機能の詳細ガイド** - 学習アルゴリズム、データ永続化 | 開発者・研究者 |
| [**LEARNING_IMPLEMENTATION.md**](./LEARNING_IMPLEMENTATION.md) | 💡 **学習システム実装詳細** - 実装手順と技術選択の理由 | 開発者 |
| [**LEARNING_DEBUG_GUIDE.md**](./LEARNING_DEBUG_GUIDE.md) | 🐛 **学習機能デバッグガイド** - トラブルシューティングとデバッグ方法 | 開発者 |

## 🎯 読む順番（推奨）

### AI実装を理解したい方
1. **AI_IMPLEMENTATION.md** - AI実装の概要
2. **EVALUATION_PATTERNS.md** - 評価パターンの詳細
3. **TECHNICAL_DOCUMENT.md** - 詳細な技術資料

### 評価関数を理解したい方
1. **EVALUATION_PATTERNS.md** - 20種類のパターン詳細
2. [窓サイズ詳細分析](../verification/WINDOW_SIZE_ANALYSIS.md) - 11マス抽出+6マス窓の分析
3. **AI_IMPLEMENTATION.md** - 評価関数の実装

### 学習システムを理解したい方
1. **LEARNING_GUIDE.md** - 学習機能の概要
2. **LEARNING_IMPLEMENTATION.md** - 実装の詳細
3. **TECHNICAL_DOCUMENT.md** の学習セクション

### 戦略理論を研究したい方
1. **DEFENSE_STRATEGY_ANALYSIS.md** - 防御戦略の理論分析
2. **EVALUATION_PATTERNS.md** - 評価パターンの詳細
3. **AI_IMPLEMENTATION.md** - AI実装の概要

### アルゴリズムを研究したい方
1. **TECHNICAL_DOCUMENT.md** - 全体像の把握
2. **AI_IMPLEMENTATION.md** - 探索アルゴリズムの詳細
3. **LEARNING_GUIDE.md** - 学習アルゴリズムの詳細
4. **EVALUATION_PATTERNS.md** - 評価パターンの詳細

## 📖 カバーされる技術

### 探索アルゴリズム
- ✅ Alpha-Beta探索
- ✅ Iterative Deepening（反復深化探索）
- ✅ 置換表（Zobrist Hashing）
- ✅ TSS（詰み探索）

### 評価関数
- ✅ ビットボード
- ✅ テンプレートマッチング
- ✅ 20種類の評価パターン
- ✅ 11マス抽出 + 6マス窓

### 学習システム
- ✅ パターンスコア自動調整
- ✅ 序盤定石学習
- ✅ データ永続化
- ✅ 学習モード切替

### データ構造
- ✅ ビットボード表現
- ✅ 置換表エントリ
- ✅ 序盤定石データベース
- ✅ 学習データ構造

## 📌 関連リンク

### 設定・運用ガイド
- [Config設定完全ガイド](../guides/CONFIG_GUIDE.md) - 全設定項目の詳細説明
- [AI学習・トレーニングガイド](../guides/TRAINING_GUIDE.md) - 学習の進め方と運用
- [ガイドドキュメント](../guides/) - 使い方・設定方法

### その他のドキュメント
- [検証・最適化レポート](../verification/) - 最適化の分析結果
- [実装検証・報告書](../reports/) - 実装完了報告

## 🏠 戻る

- [ドキュメントトップ](../) に戻る
- [プロジェクトルート](../../) に戻る
