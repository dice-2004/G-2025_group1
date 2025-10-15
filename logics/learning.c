//------------------------------------------------------------------------
// 学習モジュール: 対戦を重ねるごとに強くなる機能
//------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#ifdef _WIN32
#include <direct.h>  // _getcwd
#define getcwd _getcwd
#else
#include <unistd.h>  // getcwd
#endif
#include "learning.h"
#include "evaluation.h"
#include "main_engine.h"
#include "../config.h"

// グローバル学習データ
static GameRecord g_current_game;
static LearningStats g_stats = {0};
static int g_learning_initialized = 0;
static LearningMode g_learning_mode = LEARNING_MODE_PRACTICE;  // デフォルトは練習モード

// 学習率パラメータ
#define LEARNING_RATE 0.05
#define PATTERN_ADJUST_RATE 0.02
#define MIN_GAMES_FOR_LEARNING 5

// 早期カットオフ閾値の学習データ
typedef struct {
    int threshold;               // 現在の閾値
    int games_with_cutoff;       // カットオフを使用したゲーム数
    int wins_with_cutoff;        // カットオフ使用時の勝利数
    double cutoff_win_rate;      // カットオフ使用時の勝率
} CutoffLearningData;

static CutoffLearningData g_cutoff_learning = {8000, 0, 0, 0.0};

// パターンスコア学習データ（主要パターンのみ）
typedef struct {
    int wins;                    // 勝利数
    int losses;                  // 敗北数
    double adjustment_factor;    // 調整係数（-1.0 ～ +1.0）
} PatternScoreLearning;

// 学習対象のパターン（重要度の高い8つ）
static PatternScoreLearning g_pattern_learning[8] = {0};

// 前方宣言
static void apply_pattern_learning(void);
static void learn_opening_moves(int result);

// ========================================
// 初期化
// ========================================
void init_learning(void) {
    if (g_learning_initialized) return;

    memset(&g_current_game, 0, sizeof(GameRecord));
    memset(&g_stats, 0, sizeof(LearningStats));
    memset(&g_cutoff_learning, 0, sizeof(CutoffLearningData));
    memset(g_pattern_learning, 0, sizeof(g_pattern_learning));

    // デフォルト値設定
    g_cutoff_learning.threshold = 8000;

    g_learning_initialized = 1;
    printf("[Learning] Learning system initialized.\n");
}

// ========================================
// 対戦記録管理
// ========================================
void start_game_record(void) {
    if (!g_learning_initialized) init_learning();

    memset(&g_current_game, 0, sizeof(GameRecord));
    printf("[Learning] Game recording started (move_count reset to 0).\n");
}

void record_move(int x, int y, int color) {
    if (!g_learning_initialized) {
        printf("[Learning] WARNING: record_move called but learning not initialized!\n");
        return;
    }
    if (g_current_game.move_count >= BOARD_SIZE * BOARD_SIZE) {
        printf("[Learning] WARNING: Too many moves recorded!\n");
        return;
    }

    int idx = g_current_game.move_count;
    g_current_game.move_x[idx] = x;
    g_current_game.move_y[idx] = y;
    g_current_game.move_color[idx] = color;
    g_current_game.move_count++;

    if (g_current_game.move_count <= 10 || g_current_game.move_count % 10 == 0) {
        printf("[Learning] Recorded move #%d: (%d,%d) color=%d\n",
               g_current_game.move_count, x, y, color);
    }
}

// ========================================
// 早期カットオフ閾値の学習
// ========================================
static void learn_cutoff_threshold(int result) {
    // カットオフが使用されたかどうかを追跡
    // ここでは全てのゲームでカットオフ機能を使っていると仮定
    g_cutoff_learning.games_with_cutoff++;

    if (result == 1) {
        g_cutoff_learning.wins_with_cutoff++;
    }

    if (g_cutoff_learning.games_with_cutoff >= MIN_GAMES_FOR_LEARNING) {
        g_cutoff_learning.cutoff_win_rate =
            (double)g_cutoff_learning.wins_with_cutoff / g_cutoff_learning.games_with_cutoff;

        // 勝率に基づいて閾値を調整
        if (g_cutoff_learning.games_with_cutoff % 10 == 0) {  // 10ゲームごとに調整
            int old_threshold = g_cutoff_learning.threshold;

            if (g_cutoff_learning.cutoff_win_rate < 0.40) {
                // 勝率が低い → 閾値が高すぎて早くカットオフしすぎている可能性
                // 閾値を上げて、より慎重に判断させる
                g_cutoff_learning.threshold += 500;
            } else if (g_cutoff_learning.cutoff_win_rate > 0.60) {
                // 勝率が高い → 閾値を少し下げて効率を上げる
                g_cutoff_learning.threshold -= 200;
            }
            // 0.40～0.60の範囲なら調整しない（適切な閾値）

            // 範囲制限
            if (g_cutoff_learning.threshold < 3000) g_cutoff_learning.threshold = 3000;
            if (g_cutoff_learning.threshold > 15000) g_cutoff_learning.threshold = 15000;

            // エンジンに反映
            set_early_cutoff_threshold(g_cutoff_learning.threshold);

            if (g_config.debug_level >= 2) {
                printf("[Learning] Cutoff threshold adjusted: %d -> %d (win_rate=%.2f%%)\n",
                       old_threshold, g_cutoff_learning.threshold,
                       g_cutoff_learning.cutoff_win_rate * 100.0);
            }
        }
    }
}

// ========================================
// パターンスコアの学習
// ========================================
static void learn_pattern_scores(int result) {
    // 各パターンの勝敗を記録
    for (int i = 0; i < 8; i++) {
        if (result == 1) {
            g_pattern_learning[i].wins++;
        } else if (result == -1) {
            g_pattern_learning[i].losses++;
        }
    }

    // 一定ゲーム数ごとにスコアを調整
    if (g_stats.total_games % 20 == 0 && g_stats.total_games >= MIN_GAMES_FOR_LEARNING) {
        apply_pattern_learning();
    }
}

// ========================================
// パターンスコアの適用
// ========================================
static void apply_pattern_learning(void) {
    if (g_config.debug_level >= 2) {
        printf("[Learning] Applying pattern score adjustments...\n");
    }

    // パターンごとの勝率を計算して調整係数を更新
    for (int i = 0; i < 8; i++) {
        int total = g_pattern_learning[i].wins + g_pattern_learning[i].losses;
        if (total >= 10) {  // 最低10ゲームのデータが必要
            double win_rate = (double)g_pattern_learning[i].wins / total;

            // 勝率に基づいて調整係数を計算
            // 勝率50%を基準に、±0.5の範囲で調整
            g_pattern_learning[i].adjustment_factor = (win_rate - 0.5) * 2.0;

            // -0.5 ～ +0.5 の範囲に制限
            if (g_pattern_learning[i].adjustment_factor > 0.5)
                g_pattern_learning[i].adjustment_factor = 0.5;
            if (g_pattern_learning[i].adjustment_factor < -0.5)
                g_pattern_learning[i].adjustment_factor = -0.5;
        }
    }

    // 実際のスコアに反映（小さい調整率で）
    // パターン0: OPEN_THREE
    g_score_open_three = SCORE_OPEN_THREE +
        (int)(SCORE_OPEN_THREE * g_pattern_learning[0].adjustment_factor * PATTERN_ADJUST_RATE);

    // パターン1: BROKEN_THREE
    g_score_broken_three = SCORE_BROKEN_THREE +
        (int)(SCORE_BROKEN_THREE * g_pattern_learning[1].adjustment_factor * PATTERN_ADJUST_RATE);

    // パターン2: THREE
    g_score_three = SCORE_THREE +
        (int)(SCORE_THREE * g_pattern_learning[2].adjustment_factor * PATTERN_ADJUST_RATE);

    // パターン3: OPEN_TWO
    g_score_open_two = SCORE_OPEN_TWO +
        (int)(SCORE_OPEN_TWO * g_pattern_learning[3].adjustment_factor * PATTERN_ADJUST_RATE);

    // パターン4: TWO
    g_score_two = SCORE_TWO +
        (int)(SCORE_TWO * g_pattern_learning[4].adjustment_factor * PATTERN_ADJUST_RATE);

    // パターン5: CENTER
    g_score_center = SCORE_CENTER +
        (int)(SCORE_CENTER * g_pattern_learning[5].adjustment_factor * PATTERN_ADJUST_RATE);

    // パターン6: NEAR_ENEMY
    g_score_near_enemy = SCORE_NEAR_ENEMY +
        (int)(SCORE_NEAR_ENEMY * g_pattern_learning[6].adjustment_factor * PATTERN_ADJUST_RATE);

    // パターン7: CONNECTION
    g_score_connection = SCORE_CONNECTION +
        (int)(SCORE_CONNECTION * g_pattern_learning[7].adjustment_factor * PATTERN_ADJUST_RATE);

    if (g_config.debug_level >= 2) {
        printf("[Learning] Pattern scores adjusted:\n");
        printf("  OPEN_THREE: %d (factor=%.3f)\n", g_score_open_three, g_pattern_learning[0].adjustment_factor);
        printf("  BROKEN_THREE: %d (factor=%.3f)\n", g_score_broken_three, g_pattern_learning[1].adjustment_factor);
        printf("  THREE: %d (factor=%.3f)\n", g_score_three, g_pattern_learning[2].adjustment_factor);
        printf("  OPEN_TWO: %d (factor=%.3f)\n", g_score_open_two, g_pattern_learning[3].adjustment_factor);
    }
}

// ========================================
// 序盤定石の学習
// ========================================
static void learn_opening_moves(int result) {
    printf("[Learning] learn_opening_moves called: result=%d, move_count=%d\n",
           result, g_current_game.move_count);

    if (result != 1) {
        printf("[Learning] Skipping opening book update (not a win)\n");
        return;  // 勝った試合のみ学習
    }

    if (g_current_game.move_count < 5) {
        printf("[Learning] Skipping opening book update (too few moves: %d)\n",
               g_current_game.move_count);
        return;
    }

    // カレントディレクトリを表示
    char cwd[256];
    #ifdef _WIN32
    _getcwd(cwd, sizeof(cwd));
    #else
    getcwd(cwd, sizeof(cwd));
    #endif
    printf("[Learning] Current directory: %s\n", cwd);

    // 勝った試合の序盤5手を opening_book.csv に追加
    const char *filename = "opening_book.csv";
    printf("[Learning] Opening file %s for writing...\n", filename);

    // まずファイルが存在するかチェック
    FILE *fp_check = fopen(filename, "r");
    int file_exists = (fp_check != NULL);
    if (fp_check) {
        printf("[Learning] File exists, will append\n");
        fclose(fp_check);
    } else {
        printf("[Learning] File does not exist, will create new\n");
    }

    // 追記モードで開く
    FILE *fp = fopen(filename, "a");
    if (!fp) {
        printf("[Learning] ERROR: Could not open %s for writing!\n", filename);
        perror("[Learning] Error details");
        printf("[Learning] Trying to create directory if needed...\n");
        return;
    }

    printf("[Learning] File opened successfully (handle=%p)\n", (void*)fp);

    // ファイルが新規作成の場合はヘッダーを書く
    if (!file_exists) {
        printf("[Learning] Writing header to new file...\n");
        fprintf(fp, "move_number,x,y,comment\n");
    }

    // 勝利した試合の序盤手を追加
    printf("[Learning] Writing %d opening moves...\n",
           (g_current_game.move_count < 5) ? g_current_game.move_count : 5);

    for (int i = 0; i < 5 && i < g_current_game.move_count; i++) {
        int written = fprintf(fp, "%d,%d,%d,Learned from win (game %d)\n",
                i + 1,
                g_current_game.move_x[i],
                g_current_game.move_y[i],
                g_stats.total_games);
        printf("[Learning]   Move %d: (%d,%d) - wrote %d bytes\n",
               i+1, g_current_game.move_x[i], g_current_game.move_y[i], written);
    }

    fflush(fp);  // バッファをフラッシュ
    fclose(fp);
    printf("[Learning] Added winning opening moves to book (file closed).\n");
}

// ========================================
// 対戦終了と学習
// ========================================
void finish_game_record(int result) {
    printf("[Learning] finish_game_record called with result=%d\n", result);

    if (!g_learning_initialized) {
        printf("[Learning] ERROR: Learning not initialized!\n");
        return;
    }

    g_current_game.result = result;

    printf("\n========================================\n");
    printf("[Learning] Game finished! Result: %s\n",
           result == 1 ? "WIN" : (result == -1 ? "LOSS" : "DRAW"));
    printf("[Learning] Mode: %s\n", get_learning_mode_name());
    printf("[Learning] Move count in this game: %d\n", g_current_game.move_count);

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
    printf("[Learning] Practice mode: Learning will be performed\n");
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

    // デバッグレベル1以上で累積統計の変化を表示
    if (g_config.debug_level >= 1) {
        printf("[Learning] ========================================\n");
        printf("[Learning] Learning Progress Update:\n");
        printf("[Learning] ----------------------------------------\n");
        printf("[Learning]   This game result: %s\n",
               result == 1 ? "WIN (+1)" : (result == -1 ? "LOSS (-1)" : "DRAW (0)"));
        printf("[Learning]   Accumulated total: %d games\n", g_stats.total_games);
        printf("[Learning]   Win/Loss/Draw: %d / %d / %d\n",
               g_stats.wins, g_stats.losses, g_stats.draws);
        printf("[Learning]   Overall win rate: %.2f%%\n", g_stats.win_rate * 100.0);
        if (g_stats.total_games >= MIN_GAMES_FOR_LEARNING) {
            printf("[Learning]   Learning status: ACTIVE (>=%d games)\n", MIN_GAMES_FOR_LEARNING);
        } else {
            printf("[Learning]   Learning status: WARMING UP (need %d more games)\n",
                   MIN_GAMES_FOR_LEARNING - g_stats.total_games);
        }
        printf("[Learning] ========================================\n");
    }

    // 早期カットオフ閾値の学習
    learn_cutoff_threshold(result);

    // パターンスコアの学習
    learn_pattern_scores(result);

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
    if (g_config.debug_level >= 2) {
        printf("[Learning] Attempting to save learning data...\n");
        printf("[Learning]   Stats: total=%d, wins=%d, losses=%d, draws=%d\n",
               g_stats.total_games, g_stats.wins, g_stats.losses, g_stats.draws);

        // カレントディレクトリを表示
        char cwd[256];
        #ifdef _WIN32
        _getcwd(cwd, sizeof(cwd));
        #else
        getcwd(cwd, sizeof(cwd));
        #endif
        printf("[Learning] Current directory: %s\n", cwd);
    }

    const char *filename = "learning_data.dat";
    if (g_config.debug_level >= 2) {
        printf("[Learning] Attempting to open %s for writing...\n", filename);
    }

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        printf("[Learning] ERROR: Could not open %s for writing!\n", filename);
        perror("[Learning] Error details");
        printf("[Learning] Please check file permissions in current directory\n");
        return;
    }

    if (g_config.debug_level >= 2) {
        printf("[Learning] File opened successfully (handle=%p)\n", (void*)fp);
    }

    // 統計データ保存
    size_t written1 = fwrite(&g_stats, sizeof(LearningStats), 1, fp);
    if (g_config.debug_level >= 2) {
        printf("[Learning] Wrote stats: %lu records (%lu bytes)\n",
               (unsigned long)written1, (unsigned long)sizeof(LearningStats));
    }

    // 早期カットオフ学習データ保存
    size_t written2 = fwrite(&g_cutoff_learning, sizeof(CutoffLearningData), 1, fp);
    if (g_config.debug_level >= 2) {
        printf("[Learning] Wrote cutoff data: %lu records (%lu bytes)\n",
               (unsigned long)written2, (unsigned long)sizeof(CutoffLearningData));
    }

    // パターン学習データ保存
    size_t written3 = fwrite(g_pattern_learning, sizeof(PatternScoreLearning), 8, fp);
    if (g_config.debug_level >= 2) {
        printf("[Learning] Wrote pattern data: %lu records (%lu bytes)\n",
               (unsigned long)written3, (unsigned long)(sizeof(PatternScoreLearning) * 8));
    }

    fflush(fp);  // バッファをフラッシュ
    fclose(fp);

    // デバッグレベル1以上で保存成功と累積データを表示
    if (g_config.debug_level >= 1) {
        printf("[Learning] ========================================\n");
        printf("[Learning] Learning Data Saved Successfully\n");
        printf("[Learning] ----------------------------------------\n");
        printf("[Learning]   File: %s\n", filename);
        printf("[Learning]   Total games: %d (+1 from previous)\n", g_stats.total_games);
        printf("[Learning]   Current stats: W:%d L:%d D:%d\n",
               g_stats.wins, g_stats.losses, g_stats.draws);
        printf("[Learning]   Win rate: %.2f%%\n", g_stats.win_rate * 100.0);
        printf("[Learning] ----------------------------------------\n");
        printf("[Learning]   Data integrity:\n");
        printf("[Learning]     Stats: %s\n", written1 == 1 ? "OK" : "FAILED");
        printf("[Learning]     Cutoff: %s\n", written2 == 1 ? "OK" : "FAILED");
        printf("[Learning]     Patterns: %s\n", written3 == 8 ? "OK" : "FAILED");
        printf("[Learning] ========================================\n");
    } else if (g_config.debug_level >= 2) {
        printf("[Learning] Learning data saved successfully (file closed).\n");
    }
}

void load_learning_data(void) {
    FILE *fp = fopen("learning_data.dat", "rb");
    if (!fp) {
        if (g_config.debug_level >= 1) {
            printf("[Learning] No previous learning data found. Starting fresh.\n");
        }
        return;
    }

    if (g_config.debug_level >= 1) {
        printf("[Learning] Loading learning data from learning_data.dat...\n");
    }

    // 統計データ読み込み
    if (fread(&g_stats, sizeof(LearningStats), 1, fp) != 1) {
        printf("[Learning] Warning: Failed to load stats.\n");
        fclose(fp);
        return;
    }

    // 早期カットオフ学習データ読み込み
    if (fread(&g_cutoff_learning, sizeof(CutoffLearningData), 1, fp) != 1) {
        printf("[Learning] Warning: Failed to load cutoff data. Using defaults.\n");
        g_cutoff_learning.threshold = 8000;
        g_cutoff_learning.games_with_cutoff = 0;
        g_cutoff_learning.wins_with_cutoff = 0;
        g_cutoff_learning.cutoff_win_rate = 0.0;
    } else {
        // 読み込んだ閾値をエンジンに設定
        set_early_cutoff_threshold(g_cutoff_learning.threshold);
    }

    // パターン学習データ読み込み
    if (fread(g_pattern_learning, sizeof(PatternScoreLearning), 8, fp) != 8) {
        printf("[Learning] Warning: Failed to load pattern data. Using defaults.\n");
        memset(g_pattern_learning, 0, sizeof(g_pattern_learning));
    }

    fclose(fp);

    // デバッグレベル1以上で累積学習データを表示
    if (g_config.debug_level >= 1) {
        printf("[Learning] ========================================\n");
        printf("[Learning] Accumulated Learning Data Summary:\n");
        printf("[Learning] ----------------------------------------\n");
        printf("[Learning]   Total games played: %d\n", g_stats.total_games);
        printf("[Learning]   Wins: %d (%.2f%%)\n", g_stats.wins,
               g_stats.total_games > 0 ? (g_stats.wins * 100.0 / g_stats.total_games) : 0.0);
        printf("[Learning]   Losses: %d (%.2f%%)\n", g_stats.losses,
               g_stats.total_games > 0 ? (g_stats.losses * 100.0 / g_stats.total_games) : 0.0);
        printf("[Learning]   Draws: %d (%.2f%%)\n", g_stats.draws,
               g_stats.total_games > 0 ? (g_stats.draws * 100.0 / g_stats.total_games) : 0.0);
        printf("[Learning]   Overall win rate: %.2f%%\n", g_stats.win_rate * 100.0);
        printf("[Learning] ----------------------------------------\n");
        printf("[Learning]   Cutoff threshold: %d\n", g_cutoff_learning.threshold);
        printf("[Learning]   Games with cutoff: %d\n", g_cutoff_learning.games_with_cutoff);
        printf("[Learning]   Wins with cutoff: %d\n", g_cutoff_learning.wins_with_cutoff);
        if (g_cutoff_learning.games_with_cutoff > 0) {
            printf("[Learning]   Cutoff win rate: %.2f%%\n",
                   g_cutoff_learning.cutoff_win_rate * 100.0);
        }
        printf("[Learning] ========================================\n");
    }

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

// ========================================
// 現在のゲームの手数を取得
// ========================================
int get_current_move_count(void) {
    if (!g_learning_initialized) {
        return 0;
    }
    return g_current_game.move_count;
}
