// 学習機能のテストプログラム
// コンパイル: gcc test_learning.c logics/learning.c logics/evaluation.c config.c -o test_learning.exe

#include <stdio.h>
#include "logics/learning.h"

int main() {
    printf("=== 学習システムテスト ===\n\n");

    // 1. 初期化
    printf("1. 学習システムの初期化...\n");
    init_learning();
    load_learning_data();

    // 2. 対戦シミュレーション
    printf("\n2. 対戦シミュレーション（5試合）...\n");

    for (int i = 0; i < 5; i++) {
        printf("\n--- 試合 %d ---\n", i + 1);
        start_game_record();

        // 仮の手を記録
        record_move(7, 7, 1);
        record_move(8, 8, 2);
        record_move(7, 8, 1);
        record_move(8, 7, 2);

        // 結果（交互に勝敗）
        int result = (i % 2 == 0) ? 1 : -1;
        finish_game_record(result);
    }

    // 3. 統計確認
    printf("\n3. 学習統計の確認...\n");
    LearningStats stats = get_learning_stats();
    printf("  総試合数: %d\n", stats.total_games);
    printf("  勝利数: %d\n", stats.wins);
    printf("  敗北数: %d\n", stats.losses);
    printf("  勝率: %.2f%%\n", stats.win_rate * 100.0);

    printf("\n=== テスト完了 ===\n");
    printf("learning_data.dat が生成されました。\n");
    printf("gomoku_new.exe を起動すると、この学習データが読み込まれます。\n");

    return 0;
}
