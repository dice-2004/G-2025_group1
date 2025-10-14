#!/bin/bash
# クリーンビルドスクリプト

echo "=== Gomoku AI クリーンビルド ==="
echo ""

# オブジェクトファイルの削除
echo "既存のビルド成果物を削除中..."
rm -f *.o
rm -f logics/*.o
rm -f gomoku gomoku.exe
echo "削除完了"
echo ""

# 再ビルド
echo "再ビルド中..."
make
echo ""

if [ $? -eq 0 ]; then
    echo "=== ビルド成功 ==="
    echo "実行ファイル: gomoku"
else
    echo "=== ビルド失敗 ==="
    exit 1
fi
