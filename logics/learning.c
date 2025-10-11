//------------------------------------------------------------------------
// 学習モジュール: 対戦を重ねるごとに強くなる機能
//------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "learning.h"
#include "evaluation.h"
#include "../config.h"

// グローバル学習データ
static GameRecord g_current_game;
static LearningStats g_stats = {0};
static int g_learning_initialized = 0;
static LearningMode g_learning_mode = LEARNING_MODE_PRACTICE;  // デフォルトは練習モード

// 学習率パラメータ
#define LEARNING_RATE 0.05
#define PATTERN_ADJUST_RATE 0.10
#define MIN_GAMES_FOR_LEARNING 5

// パターンスコア調整用の累積データ
typedef struct {
    double score_adjustment[8];  // 8つのパターンスコアの調整値
    int pattern_success[8];      // パターンごとの成功回数
    int pattern_failure[8];      // パターンごとの失敗回数
} PatternLearningData;

static PatternLearningData g_pattern_learning = {0};

// ========================================
// 初期化
// ========================================
void init_learning(void) {
    if (g_learning_initialized) return;

    memset(&g_current_game, 0, sizeof(GameRecord));
    memset(&g_stats, 0, sizeof(LearningStats));
    memset(&g_pattern_learning, 0, sizeof(PatternLearningData));

    g_learning_initialized = 1;
    printf("[Learning] Learning system initialized.\n");
}

// ========================================
// 対戦記録管理
// ========================================
void start_game_record(void) {
    if (!g_learning_initialized) init_learning();

    memset(&g_current_game, 0, sizeof(GameRecord));
    printf("[Learning] Game recording started.\n");
}

void record_move(int x, int y, int color) {
    if (!g_learning_initialized) return;
    if (g_current_game.move_count >= BOARD_SIZE * BOARD_SIZE) return;

    int idx = g_current_game.move_count;
    g_current_game.move_x[idx] = x;
    g_current_game.move_y[idx] = y;
    g_current_game.move_color[idx] = color;
    g_current_game.move_count++;
}

// ========================================
// パターン分析と学習
// ========================================
static void analyze_game_patterns(int result) {
    // 勝った場合は使ったパターンを強化、負けた場合は弱化
    double factor = (result == 1) ? PATTERN_ADJUST_RATE : -PATTERN_ADJUST_RATE;

    // 簡易的なパターン分析（盤面の特徴から推定）
    // 実戦で使われたパターンを推定し、調整値を累積

    if (g_current_game.move_count > 10) {
        // 序盤（0-10手）のパターン
        g_pattern_learning.score_adjustment[7] += factor * 0.5;  // ONE
        g_pattern_learning.score_adjustment[6] += factor * 0.8;  // TWO

        if (result == 1) {
            g_pattern_learning.pattern_success[7]++;
            g_pattern_learning.pattern_success[6]++;
        } else if (result == -1) {
            g_pattern_learning.pattern_failure[7]++;
            g_pattern_learning.pattern_failure[6]++;
        }
    }

    if (g_current_game.move_count > 20) {
        // 中盤（11-20手）のパターン
        g_pattern_learning.score_adjustment[5] += factor * 1.0;  // OPEN_TWO
        g_pattern_learning.score_adjustment[4] += factor * 1.5;  // THREE
        g_pattern_learning.score_adjustment[3] += factor * 2.0;  // OPEN_THREE

        if (result == 1) {
            g_pattern_learning.pattern_success[5]++;
            g_pattern_learning.pattern_success[4]++;
            g_pattern_learning.pattern_success[3]++;
        } else if (result == -1) {
            g_pattern_learning.pattern_failure[5]++;
            g_pattern_learning.pattern_failure[4]++;
            g_pattern_learning.pattern_failure[3]++;
        }
    }

    if (g_current_game.move_count > 30) {
        // 終盤（21手以降）のパターン
        g_pattern_learning.score_adjustment[2] += factor * 3.0;  // FOUR
        g_pattern_learning.score_adjustment[1] += factor * 5.0;  // OPEN_FOUR

        if (result == 1) {
            g_pattern_learning.pattern_success[2]++;
            g_pattern_learning.pattern_success[1]++;
        } else if (result == -1) {
            g_pattern_learning.pattern_failure[2]++;
            g_pattern_learning.pattern_failure[1]++;
        }
    }
}

// ========================================
// パターンスコアの適用
// ========================================
static void apply_pattern_learning(void) {
    if (g_stats.total_games < MIN_GAMES_FOR_LEARNING) {
        printf("[Learning] Not enough games for learning (need %d, have %d)\n",
               MIN_GAMES_FOR_LEARNING, g_stats.total_games);
        return;
    }

    // 学習率を考慮してパターンスコアを調整
    double learning_factor = LEARNING_RATE * (1.0 / sqrt(g_stats.total_games));

    printf("[Learning] Applying pattern adjustments (factor=%.4f)...\n", learning_factor);

    // 各パターンスコアを調整
    g_score_five += (int)(g_pattern_learning.score_adjustment[0] * learning_factor);
    g_score_open_four += (int)(g_pattern_learning.score_adjustment[1] * learning_factor);
    g_score_four += (int)(g_pattern_learning.score_adjustment[2] * learning_factor);
    g_score_open_three += (int)(g_pattern_learning.score_adjustment[3] * learning_factor);
    g_score_three += (int)(g_pattern_learning.score_adjustment[4] * learning_factor);
    g_score_open_two += (int)(g_pattern_learning.score_adjustment[5] * learning_factor);
    g_score_two += (int)(g_pattern_learning.score_adjustment[6] * learning_factor);
    g_score_one += (int)(g_pattern_learning.score_adjustment[7] * learning_factor);

    printf("[Learning] Pattern scores updated:\n");
    printf("  FIVE: %d, OPEN_FOUR: %d, FOUR: %d\n", g_score_five, g_score_open_four, g_score_four);
    printf("  OPEN_THREE: %d, THREE: %d, OPEN_TWO: %d\n", g_score_open_three, g_score_three, g_score_open_two);
    printf("  TWO: %d, ONE: %d\n", g_score_two, g_score_one);
}

// ========================================
// 序盤定石の学習
// ========================================
static void learn_opening_moves(int result) {
    if (result != 1) return;  // 勝った試合のみ学習
    if (g_current_game.move_count < 5) return;

    // 勝った試合の序盤5手を opening_book.csv に追加
    FILE *fp = fopen("opening_book.csv", "a");
    if (!fp) return;

    // ヘッダー行が存在するかチェック
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);

    if (file_size == 0) {
        // ファイルが空の場合はヘッダーを書く
        fprintf(fp, "move_number,x,y,comment\n");
    }

    // 勝利した試合の序盤手を追加
    for (int i = 0; i < 5 && i < g_current_game.move_count; i++) {
        fprintf(fp, "%d,%d,%d,Learned from win (game %d)\n",
                i + 1,
                g_current_game.move_x[i],
                g_current_game.move_y[i],
                g_stats.total_games);
    }

    fclose(fp);
    printf("[Learning] Added winning opening moves to book.\n");
}

// ========================================
// 対戦終了と学習
// ========================================
void finish_game_record(int result) {
    if (!g_learning_initialized) return;

    g_current_game.result = result;

    printf("\n========================================\n");
    printf("[Learning] Game finished! Result: %s\n",
           result == 1 ? "WIN" : (result == -1 ? "LOSS" : "DRAW"));
    printf("[Learning] Mode: %s\n", get_learning_mode_name());

    // 本番モードの場合は統計のみ表示して終了（学習しない）
    if (g_learning_mode == LEARNING_MODE_PRODUCTION) {
        printf("[Learning] Production mode: No learning performed.\n");
        printf("[Learning] Current stats - Total games: %d (W:%d L:%d D:%d)\n",
               g_stats.total_games, g_stats.wins, g_stats.losses, g_stats.draws);
        printf("[Learning] Win rate: %.2f%%\n", g_stats.win_rate * 100.0);
        printf("========================================\n\n");
        return;  // 学習せずに終了
    }

    // 練習モードの場合は学習を実行
    g_stats.total_games++;

    if (result == 1) {
        g_stats.wins++;
    } else if (result == -1) {
        g_stats.losses++;
    } else {
        g_stats.draws++;
    }

    g_stats.win_rate = (g_stats.total_games > 0) ?
                       ((double)g_stats.wins / g_stats.total_games) : 0.0;

    printf("[Learning] Total games: %d (W:%d L:%d D:%d)\n",
           g_stats.total_games, g_stats.wins, g_stats.losses, g_stats.draws);
    printf("[Learning] Win rate: %.2f%%\n", g_stats.win_rate * 100.0);

    // パターン分析
    analyze_game_patterns(result);

    // 学習の適用（一定ゲーム数ごと）
    if (g_stats.total_games % 3 == 0) {  // 3試合ごとに学習
        apply_pattern_learning();
    }

    // 序盤定石の学習
    learn_opening_moves(result);

    // 学習データの保存
    save_learning_data();

    printf("========================================\n\n");
}

// ========================================
// 統計取得
// ========================================
LearningStats get_learning_stats(void) {
    return g_stats;
}

// ========================================
// 学習データの保存・読み込み
// ========================================
void save_learning_data(void) {
    FILE *fp = fopen("learning_data.dat", "wb");
    if (!fp) {
        printf("[Learning] Warning: Could not save learning data.\n");
        return;
    }

    // 統計データ保存
    fwrite(&g_stats, sizeof(LearningStats), 1, fp);

    // パターン学習データ保存
    fwrite(&g_pattern_learning, sizeof(PatternLearningData), 1, fp);

    fclose(fp);
    printf("[Learning] Learning data saved successfully.\n");
}

void load_learning_data(void) {
    FILE *fp = fopen("learning_data.dat", "rb");
    if (!fp) {
        printf("[Learning] No previous learning data found. Starting fresh.\n");
        return;
    }

    // 統計データ読み込み
    if (fread(&g_stats, sizeof(LearningStats), 1, fp) != 1) {
        printf("[Learning] Warning: Failed to load stats.\n");
        fclose(fp);
        return;
    }

    // パターン学習データ読み込み
    if (fread(&g_pattern_learning, sizeof(PatternLearningData), 1, fp) != 1) {
        printf("[Learning] Warning: Failed to load pattern data.\n");
        fclose(fp);
        return;
    }

    fclose(fp);

    printf("[Learning] Loaded previous learning data:\n");
    printf("  Total games: %d (W:%d L:%d D:%d)\n",
           g_stats.total_games, g_stats.wins, g_stats.losses, g_stats.draws);
    printf("  Win rate: %.2f%%\n", g_stats.win_rate * 100.0);

    // 学習済みのパターン調整を即座に適用
    if (g_stats.total_games >= MIN_GAMES_FOR_LEARNING) {
        apply_pattern_learning();
    }
}

// ========================================
// 学習モード管理
// ========================================
void set_learning_mode(LearningMode mode) {
    g_learning_mode = mode;

    const char* mode_name = (mode == LEARNING_MODE_PRODUCTION) ? "PRODUCTION (本番)" : "PRACTICE (練習)";
    printf("[Learning] Learning mode set to: %s\n", mode_name);

    if (mode == LEARNING_MODE_PRODUCTION) {
        printf("[Learning] Production mode: Learning disabled, using existing data only.\n");
    } else {
        printf("[Learning] Practice mode: Learning enabled, data will be updated.\n");
    }
}

LearningMode get_learning_mode(void) {
    return g_learning_mode;
}

const char* get_learning_mode_name(void) {
    return (g_learning_mode == LEARNING_MODE_PRODUCTION) ? "PRODUCTION" : "PRACTICE";
}
