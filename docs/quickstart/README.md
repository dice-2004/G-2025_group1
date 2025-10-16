# 📕 クイックスタート・プレゼン

すぐに始めたい方、プロジェクトの概要を知りたい方向けのドキュメントを集約したフォルダです。

## 📚 ドキュメント一覧

### クイックスタート

| ファイル | 説明 | 所要時間 | 対象者 |
|---------|------|---------|--------|
| [**QUICKSTART.md**](./QUICKSTART.md) | 🚀 **クイックスタートガイド** - 5分で始める五目並べAI | 5分 | 全員 |

### プレゼンテーション

| ファイル | 説明 | スライド数 | 対象者 |
|---------|------|-----------|--------|
| [**PRESENTATION_SLIDES.md**](./PRESENTATION_SLIDES.md) | 📊 **プレゼン用スライド** - プロジェクト全体の概要 | 30枚 | 全員 |

## 🎯 どちらを読むべきか？

### すぐに使い始めたい方
→ **QUICKSTART.md**
- ビルド方法
- 実行方法
- 基本的な使い方
- 設定のカスタマイズ

### プロジェクトの全体像を知りたい方
→ **PRESENTATION_SLIDES.md**
- プロジェクト概要
- 主要機能
- 技術的特徴
- 実装の成果
- デモ画面

## 🚀 クイックスタート（概要）

### 1. ビルド

**Windows:**
```cmd
build.bat
```

**Linux/Mac/MSYS2:**
```bash
make all
```

### 2. 実行

```cmd
gomoku_new.exe
```

### 3. 設定カスタマイズ

`config.txt` を編集：
- `SEARCH_DEPTH`: 探索深度（1-8）
- `LEARNING_MODE`: 学習モード（PRACTICE/PRODUCTION）
- `DEBUG_LEVEL`: デバッグレベル（0-3）
- `ID_TIME_THRESHOLD`: 時間閾値（0.0-1.0）

詳しくは **QUICKSTART.md** をご覧ください。

## 📊 プレゼンスライド（概要）

### スライド構成（30枚）

1. **タイトル・概要**（1-5枚）
2. **主要機能**（6-10枚）
   - 学習機能
   - デバッグ機能
   - 最適化成果
3. **技術的特徴**（11-20枚）
   - Alpha-Beta探索
   - 評価関数
   - 11マス抽出 + 6マス窓
4. **実装の成果**（21-25枚）
   - バージョン履歴
   - 検証結果
5. **まとめ・今後の展開**（26-30枚）

詳しくは **PRESENTATION_SLIDES.md** をご覧ください。

## 📌 次のステップ

### 設定をカスタマイズしたい方
- [Config設定完全ガイド](../guides/CONFIG_GUIDE.md) - 全設定項目の詳細説明
- [使い方ガイド](../guides/USAGE_GUIDE.md) - ビルド・実行・設定方法

### AI学習を行いたい方
- [AI学習・トレーニングガイド](../guides/TRAINING_GUIDE.md) - 学習の進め方と運用
- [学習モード切替ガイド](../guides/LEARNING_MODE_GUIDE.md) - 練習・本番モードの切替

### さらに詳しく知りたい方
- [ガイドドキュメント](../guides/) - 各機能の詳細な使い方
- [技術ドキュメント](../technical/) - アルゴリズムの詳細
- [検証・最適化レポート](../verification/) - 最適化の分析結果

### 開発に参加したい方
- [技術ドキュメント](../technical/) - 実装の詳細
- [実装検証・報告書](../reports/) - 各バージョンの実装記録
- [ガイドドキュメント](../guides/) - ビルド方法

## 🎓 学習パス

### 初心者向け
1. **QUICKSTART.md** - まず使ってみる
2. [SYSTEM_BEHAVIOR_GUIDE.md](../guides/SYSTEM_BEHAVIOR_GUIDE.md) - システムの理解
3. [LEARNING_MODE_GUIDE.md](../guides/LEARNING_MODE_GUIDE.md) - 学習モードの活用

### 中級者向け
1. **PRESENTATION_SLIDES.md** - 全体像の把握
2. [AI_IMPLEMENTATION.md](../technical/AI_IMPLEMENTATION.md) - AI実装の理解
3. [DEBUG_AND_ID_GUIDE.md](../guides/DEBUG_AND_ID_GUIDE.md) - 詳細設定

### 上級者・開発者向け
1. **PRESENTATION_SLIDES.md** - プロジェクト概要
2. [TECHNICAL_DOCUMENT.md](../technical/TECHNICAL_DOCUMENT.md) - 完全技術資料
3. [WINDOW_SIZE_ANALYSIS.md](../verification/WINDOW_SIZE_ANALYSIS.md) - 最適化の詳細

## 🏠 戻る

- [ドキュメントトップ](../) に戻る
- [プロジェクトルート](../../) に戻る
