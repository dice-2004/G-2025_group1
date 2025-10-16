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
#include "opening_book.h"
#include "../config.h"

// グローバル学習データ
static GameRecord g_current_game;
static LearningStats g_stats = {0};
static int g_learning_initialized = 0;
static LearningMode g_learning_mode = LEARNING_MODE_PRACTICE;  // デフォルトは練習モード

// 学習率パラメータ
#define LEARNING_RATE 0.15           // 0.05→0.15に増加（より大きな学習効果）
#define PATTERN_ADJUST_RATE 0.20     // 0.05→0.20に増加（勝率に応じて最大±20%調整）
#define MIN_GAMES_FOR_LEARNING 3     // 3ゲームで学習開始
#define MAX_ADJUSTMENT_FACTOR 1.5    // 調整係数の最大値（±1.5倍まで）

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
    g_current_game.opening_game_id = -1;  // 初期値
    g_current_game.ai_color = STONE_SPACE;  // まだ不明
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

    // 1手目で定石のゲームIDを記録
    if (g_current_game.move_count == 1) {
        g_current_game.opening_game_id = get_selected_game_id();
        if (g_current_game.opening_game_id >= 0) {
            printf("[Learning] Opening game ID %d will be tracked for this game\n",
                   g_current_game.opening_game_id);
        }
    }

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

        // 勝率に基づいて閾値を調整（5ゲームごとに調整して即応性を高める）
        if (g_cutoff_learning.games_with_cutoff % 5 == 0) {
            int old_threshold = g_cutoff_learning.threshold;

            // より積極的な調整（勝率に応じて大きく変更）
            if (g_cutoff_learning.cutoff_win_rate < 0.35) {
                // 勝率が非常に低い → 閾値を大きく上げて慎重に
                g_cutoff_learning.threshold += 1000;
            } else if (g_cutoff_learning.cutoff_win_rate < 0.45) {
                // 勝率が低い → 閾値を上げる
                g_cutoff_learning.threshold += 500;
            } else if (g_cutoff_learning.cutoff_win_rate > 0.70) {
                // 勝率が非常に高い → 閾値を大きく下げて効率化
                g_cutoff_learning.threshold -= 800;
            } else if (g_cutoff_learning.cutoff_win_rate > 0.60) {
                // 勝率が高い → 閾値を下げる
                g_cutoff_learning.threshold -= 400;
            }
            // 0.45～0.60の範囲なら調整しない（適切な閾値）

            // 範囲制限（より広い範囲を許容）
            if (g_cutoff_learning.threshold < 2000) g_cutoff_learning.threshold = 2000;
            if (g_cutoff_learning.threshold > 20000) g_cutoff_learning.threshold = 20000;

            // エンジンに反映
            set_early_cutoff_threshold(g_cutoff_learning.threshold);

            if (g_config.debug_level >= 1) {
                printf("[Learning] Cutoff threshold adjusted: %d -> %d (Δ=%+d, win_rate=%.2f%%)\n",
                       old_threshold, g_cutoff_learning.threshold,
                       g_cutoff_learning.threshold - old_threshold,
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

    // 毎ゲーム学習を適用（データが十分な場合）
    if (g_stats.total_games >= MIN_GAMES_FOR_LEARNING) {
        // 学習頻度を高めて即座に適応
        apply_pattern_learning();

        if (g_config.debug_level >= 1) {
            printf("[Learning] Pattern scores adjusted after game #%d\n", g_stats.total_games);
        }
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
        if (total >= 3) {  // 最低3ゲームのデータがあれば調整開始
            double win_rate = (double)g_pattern_learning[i].wins / total;

            // 勝率に基づいて調整係数を計算
            // 勝率50%を基準に、±MAX_ADJUSTMENT_FACTOR の範囲で調整
            // 例: 勝率100% → factor = 1.5 (150%ボーナス)
            //     勝率0%   → factor = -1.5 (150%ペナルティ)
            //     勝率50%  → factor = 0.0 (調整なし)
            g_pattern_learning[i].adjustment_factor = (win_rate - 0.5) * (MAX_ADJUSTMENT_FACTOR * 2.0);

            // -MAX_ADJUSTMENT_FACTOR ～ +MAX_ADJUSTMENT_FACTOR の範囲に制限
            if (g_pattern_learning[i].adjustment_factor > MAX_ADJUSTMENT_FACTOR)
                g_pattern_learning[i].adjustment_factor = MAX_ADJUSTMENT_FACTOR;
            if (g_pattern_learning[i].adjustment_factor < -MAX_ADJUSTMENT_FACTOR)
                g_pattern_learning[i].adjustment_factor = -MAX_ADJUSTMENT_FACTOR;

            // デバッグレベル2: 調整係数の計算過程を表示
            if (g_config.debug_level >= 2) {
                printf("[Learning]   Pattern %d: wins=%d, losses=%d, total=%d, win_rate=%.2f%%, factor=%.3f\n",
                       i, g_pattern_learning[i].wins, g_pattern_learning[i].losses,
                       total, win_rate * 100.0, g_pattern_learning[i].adjustment_factor);
            }
        } else {
            // データ不足の場合は調整係数を0に保つ
            g_pattern_learning[i].adjustment_factor = 0.0;
        }
    }

    // 実際のスコアに反映（調整率を適用）
    // パターン0: OPEN_THREE
    int old_open_three = g_score_open_three;
    g_score_open_three = SCORE_OPEN_THREE +
        (int)(SCORE_OPEN_THREE * g_pattern_learning[0].adjustment_factor * PATTERN_ADJUST_RATE);

    // パターン1: BROKEN_THREE
    int old_broken_three = g_score_broken_three;
    g_score_broken_three = SCORE_BROKEN_THREE +
        (int)(SCORE_BROKEN_THREE * g_pattern_learning[1].adjustment_factor * PATTERN_ADJUST_RATE);

    // パターン2: THREE
    int old_three = g_score_three;
    g_score_three = SCORE_THREE +
        (int)(SCORE_THREE * g_pattern_learning[2].adjustment_factor * PATTERN_ADJUST_RATE);

    // パターン3: OPEN_TWO
    int old_open_two = g_score_open_two;
    g_score_open_two = SCORE_OPEN_TWO +
        (int)(SCORE_OPEN_TWO * g_pattern_learning[3].adjustment_factor * PATTERN_ADJUST_RATE);

    // パターン4: TWO
    int old_two = g_score_two;
    g_score_two = SCORE_TWO +
        (int)(SCORE_TWO * g_pattern_learning[4].adjustment_factor * PATTERN_ADJUST_RATE);

    // パターン5: CENTER
    int old_center = g_score_center;
    g_score_center = SCORE_CENTER +
        (int)(SCORE_CENTER * g_pattern_learning[5].adjustment_factor * PATTERN_ADJUST_RATE);

    // パターン6: NEAR_ENEMY
    int old_near_enemy = g_score_near_enemy;
    g_score_near_enemy = SCORE_NEAR_ENEMY +
        (int)(SCORE_NEAR_ENEMY * g_pattern_learning[6].adjustment_factor * PATTERN_ADJUST_RATE);

    // パターン7: CONNECTION
    int old_connection = g_score_connection;
    g_score_connection = SCORE_CONNECTION +
        (int)(SCORE_CONNECTION * g_pattern_learning[7].adjustment_factor * PATTERN_ADJUST_RATE);

    // デバッグレベル1: 学習データによるパターンスコア調整を表示
    if (g_config.debug_level >= 1) {
        printf("[Learning] ========================================\n");
        printf("[Learning] DEBUG_LEVEL=1: Pattern Scores Adjusted by Learning:\n");
        printf("[Learning] ----------------------------------------\n");
        printf("  [0] OPEN_THREE: %d->%d (base=%d, Δ=%+d, factor=%.3f, W/L=%d/%d)\n",
               old_open_three, g_score_open_three, SCORE_OPEN_THREE,
               g_score_open_three - SCORE_OPEN_THREE,
               g_pattern_learning[0].adjustment_factor,
               g_pattern_learning[0].wins, g_pattern_learning[0].losses);
        printf("  [1] BROKEN_THREE: %d->%d (base=%d, Δ=%+d, factor=%.3f, W/L=%d/%d)\n",
               old_broken_three, g_score_broken_three, SCORE_BROKEN_THREE,
               g_score_broken_three - SCORE_BROKEN_THREE,
               g_pattern_learning[1].adjustment_factor,
               g_pattern_learning[1].wins, g_pattern_learning[1].losses);
        printf("  [2] THREE: %d->%d (base=%d, Δ=%+d, factor=%.3f, W/L=%d/%d)\n",
               old_three, g_score_three, SCORE_THREE,
               g_score_three - SCORE_THREE,
               g_pattern_learning[2].adjustment_factor,
               g_pattern_learning[2].wins, g_pattern_learning[2].losses);
        printf("  [3] OPEN_TWO: %d->%d (base=%d, Δ=%+d, factor=%.3f, W/L=%d/%d)\n",
               old_open_two, g_score_open_two, SCORE_OPEN_TWO,
               g_score_open_two - SCORE_OPEN_TWO,
               g_pattern_learning[3].adjustment_factor,
               g_pattern_learning[3].wins, g_pattern_learning[3].losses);
        printf("  [4] TWO: %d->%d (base=%d, Δ=%+d, factor=%.3f, W/L=%d/%d)\n",
               old_two, g_score_two, SCORE_TWO,
               g_score_two - SCORE_TWO,
               g_pattern_learning[4].adjustment_factor,
               g_pattern_learning[4].wins, g_pattern_learning[4].losses);
        printf("  [5] CENTER: %d->%d (base=%d, Δ=%+d, factor=%.3f, W/L=%d/%d)\n",
               old_center, g_score_center, SCORE_CENTER,
               g_score_center - SCORE_CENTER,
               g_pattern_learning[5].adjustment_factor,
               g_pattern_learning[5].wins, g_pattern_learning[5].losses);
        printf("  [6] NEAR_ENEMY: %d->%d (base=%d, Δ=%+d, factor=%.3f, W/L=%d/%d)\n",
               old_near_enemy, g_score_near_enemy, SCORE_NEAR_ENEMY,
               g_score_near_enemy - SCORE_NEAR_ENEMY,
               g_pattern_learning[6].adjustment_factor,
               g_pattern_learning[6].wins, g_pattern_learning[6].losses);
        printf("  [7] CONNECTION: %d->%d (base=%d, Δ=%+d, factor=%.3f, W/L=%d/%d)\n",
               old_connection, g_score_connection, SCORE_CONNECTION,
               g_score_connection - SCORE_CONNECTION,
               g_pattern_learning[7].adjustment_factor,
               g_pattern_learning[7].wins, g_pattern_learning[7].losses);
        printf("[Learning] ========================================\n");
    } else if (g_config.debug_level >= 2) {
        printf("[Learning] Pattern scores adjusted:\n");
        printf("  OPEN_THREE: %d (factor=%.3f)\n", g_score_open_three, g_pattern_learning[0].adjustment_factor);
        printf("  BROKEN_THREE: %d (factor=%.3f)\n", g_score_broken_three, g_pattern_learning[1].adjustment_factor);
        printf("  THREE: %d (factor=%.3f)\n", g_score_three, g_pattern_learning[2].adjustment_factor);
        printf("  OPEN_TWO: %d (factor=%.3f)\n", g_score_open_two, g_pattern_learning[3].adjustment_factor);
    }
}

// ========================================
// 低勝率定石削除機能
// ========================================
// 指定された色（黒または白）の学習定石をカウント
static int count_learned_openings(int player_color) {
    FILE *fp = fopen("opening_book.csv", "r");
    if (!fp) return 0;

    char line[512];
    int game_ids[1000] = {0};  // 最大999までのgame_idを追跡
    int count = 0;

    // ヘッダー行をスキップ
    if (fgets(line, sizeof(line), fp)) {
        // ヘッダー行
    }

    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        int game_id, move_number;
        char color_str[10];
        int x, y;
        float win_rate;

        if (sscanf(line, "%d,%d,%[^,],%d,%d,%f", &game_id, &move_number, color_str, &x, &y, &win_rate) == 6) {
            // 学習定石の範囲をチェック（重複なし設計）
            int is_learning_opening = 0;
            if (player_color == STONE_BLACK && game_id >= 200 && game_id <= 599) {
                // 黒番学習定石: 200-599
                is_learning_opening = 1;
            } else if (player_color == STONE_WHITE && game_id >= 600 && game_id <= 999) {
                // 白番学習定石: 600-999
                is_learning_opening = 1;
            }

            if (is_learning_opening && move_number == 1 && !game_ids[game_id]) {
                game_ids[game_id] = 1;
                count++;
            }
        }
    }

    fclose(fp);
    return count;
}

// 最も勝率の低い学習定石を削除
static void remove_lowest_winrate_opening(int player_color) {
    FILE *fp = fopen("opening_book.csv", "r");
    if (!fp) {
        printf("[Learning] ERROR: Could not open opening_book.csv for reading\n");
        return;
    }

    char lines[50000][512];  // 最大50000行
    int line_count = 0;
    int lowest_game_id = -1;
    float lowest_win_rate = 2.0f;  // 勝率は0.0-1.0なので2.0で初期化

    // 全行を読み込み、最低勝率のgame_idを探す
    while (fgets(lines[line_count], sizeof(lines[0]), fp) && line_count < 50000) {
        char *line = lines[line_count];

        if (line[0] == '#' || line[0] == '\n' || line_count == 0) {
            line_count++;
            continue;
        }

        int game_id, move_number;
        char color_str[10];
        int x, y;
        float win_rate;

        if (sscanf(line, "%d,%d,%[^,],%d,%d,%f", &game_id, &move_number, color_str, &x, &y, &win_rate) == 6) {
            // 学習定石の範囲をチェック（重複なし設計）
            int is_target = 0;
            if (player_color == STONE_BLACK && game_id >= 200 && game_id <= 599) {
                // 黒番学習定石: 200-599
                is_target = 1;
            } else if (player_color == STONE_WHITE && game_id >= 600 && game_id <= 999) {
                // 白番学習定石: 600-999
                is_target = 1;
            }

            if (is_target && move_number == 1) {
                if (win_rate < lowest_win_rate) {
                    lowest_win_rate = win_rate;
                    lowest_game_id = game_id;
                }
            }
        }

        line_count++;
    }
    fclose(fp);

    if (lowest_game_id == -1) {
        printf("[Learning] No opening to remove found\n");
        return;
    }

    printf("[Learning] Removing game_id %d (win_rate=%.3f) to make space\n",
           lowest_game_id, lowest_win_rate);

    // 対象game_idを除外して書き戻し
    fp = fopen("opening_book.csv", "w");
    if (!fp) {
        printf("[Learning] ERROR: Could not open opening_book.csv for writing\n");
        return;
    }

    for (int i = 0; i < line_count; i++) {
        int game_id;
        if (sscanf(lines[i], "%d,", &game_id) == 1 && game_id == lowest_game_id) {
            // この行はスキップ（削除）
            continue;
        }
        fputs(lines[i], fp);
    }

    fclose(fp);
    printf("[Learning] Successfully removed game_id %d\n", lowest_game_id);
}

// ========================================
// 序盤定石の学習
// ========================================
static void learn_opening_moves(int result) {
    printf("[Learning] learn_opening_moves called: result=%d, move_count=%d\n",
           result, g_current_game.move_count);

    // 勝った場合: 自分の手を学習
    // 負けた場合: 相手の手を学習
    // 引き分け: 学習しない
    if (result == 0) {
        printf("[Learning] Skipping opening book update (draw)\n");
        return;
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
        fprintf(fp, "# game_id,move_number,player_color,x,y,win_rate,comment\n");
    }

    // 勝利した場合: 両者の手を10手分記録（AIの観点から）
    // 敗北した場合: 相手を基準に両者の手を10手分記録
    // ゲームID規則: 手動1-49/51-99, プロ101-149/151-199, 学習200-599/600-999

    int ai_color = g_current_game.ai_color;
    int base_game_id = g_stats.total_games;
    int game_id;
    const char* opening_type;

    if (result == 1) {
        // 勝利 - AIの色に応じてゲームIDを決定（重複なし設計）
        if (ai_color == STONE_BLACK) {
            // AIが黒番で勝利 → 黒番用定石（200-599）
            game_id = (base_game_id % 400) + 200;
            opening_type = "BLACK opening";

            // スロット満杯チェック（黒番: 上限400）
            int current_count = count_learned_openings(STONE_BLACK);
            if (current_count >= 400) {
                printf("[Learning] BLACK opening slots full (%d/400), removing lowest win-rate opening\n", current_count);
                remove_lowest_winrate_opening(STONE_BLACK);
            }
        } else {
            // AIが白番で勝利 → 白番用定石（600-999）
            game_id = (base_game_id % 400) + 600;
            opening_type = "WHITE opening";

            // スロット満杯チェック（白番: 上限400）
            int current_count = count_learned_openings(STONE_WHITE);
            if (current_count >= 400) {
                printf("[Learning] WHITE opening slots full (%d/400), removing lowest win-rate opening\n", current_count);
                remove_lowest_winrate_opening(STONE_WHITE);
            }
        }

        printf("[Learning] Writing opening sequence (WIN) - %s...\n", opening_type);
        printf("[Learning] AI color: %s, Game ID: %d\n",
               ai_color == STONE_BLACK ? "BLACK" : "WHITE", game_id);

        // 最初の10手を記録
        int moves_to_record = (g_current_game.move_count < 10) ? g_current_game.move_count : 10;
        for (int i = 0; i < moves_to_record; i++) {
            int move_number = i + 1;
            // 手数から色を自動計算（奇数手=黒、偶数手=白）
            int player_color = (move_number % 2 == 1) ? STONE_BLACK : STONE_WHITE;
            const char* color_str = (player_color == STONE_BLACK) ? "BLACK" : "WHITE";

            int written = fprintf(fp, "%d,%d,%s,%d,%d,0.50,Learned from %s win (game %d)\n",
                    game_id,
                    move_number,
                    color_str,
                    g_current_game.move_x[i],
                    g_current_game.move_y[i],
                    opening_type,
                    base_game_id);
            printf("[Learning]   Move %d (%s): (%d,%d) - wrote %d bytes\n",
                   move_number, color_str,
                   g_current_game.move_x[i], g_current_game.move_y[i], written);
        }

        printf("[Learning] Learned %d moves as %s\n", moves_to_record, opening_type);

    } else if (result == -1) {
        // 敗北 - 相手の色に応じてゲームIDを決定（相手の視点で学習）
        int opponent_color = (ai_color == STONE_BLACK) ? STONE_WHITE : STONE_BLACK;

        if (opponent_color == STONE_BLACK) {
            // 相手が黒番で勝利 → 黒番用定石（200-599）
            game_id = (base_game_id % 400) + 200;
            opening_type = "BLACK opening";

            // スロット満杯チェック（黒番: 上限400）
            int current_count = count_learned_openings(STONE_BLACK);
            if (current_count >= 400) {
                printf("[Learning] BLACK opening slots full (%d/400), removing lowest win-rate opening\n", current_count);
                remove_lowest_winrate_opening(STONE_BLACK);
            }
        } else {
            // 相手が白番で勝利 → 白番用定石（600-999）
            game_id = (base_game_id % 400) + 600;
            opening_type = "WHITE opening";

            // スロット満杯チェック（白番: 上限400）
            int current_count = count_learned_openings(STONE_WHITE);
            if (current_count >= 400) {
                printf("[Learning] WHITE opening slots full (%d/400), removing lowest win-rate opening\n", current_count);
                remove_lowest_winrate_opening(STONE_WHITE);
            }
        }

        printf("[Learning] Writing opening sequence (LOSS) - learning from opponent...\n");
        printf("[Learning] Opponent color: %s, Game ID: %d\n",
               opponent_color == STONE_BLACK ? "BLACK" : "WHITE", game_id);

        // 最初の10手を記録（相手の視点で）
        int moves_to_record = (g_current_game.move_count < 10) ? g_current_game.move_count : 10;
        for (int i = 0; i < moves_to_record; i++) {
            int move_number = i + 1;
            // 手数から色を自動計算（奇数手=黒、偶数手=白）
            int player_color = (move_number % 2 == 1) ? STONE_BLACK : STONE_WHITE;
            const char* color_str = (player_color == STONE_BLACK) ? "BLACK" : "WHITE";

            int written = fprintf(fp, "%d,%d,%s,%d,%d,0.50,Learned from %s opponent (game %d)\n",
                    game_id,
                    move_number,
                    color_str,
                    g_current_game.move_x[i],
                    g_current_game.move_y[i],
                    opening_type,
                    base_game_id);
            printf("[Learning]   Move %d (%s): (%d,%d) - wrote %d bytes\n",
                   move_number, color_str,
                   g_current_game.move_x[i], g_current_game.move_y[i], written);
        }

        printf("[Learning] Learned %d moves as %s from opponent\n", moves_to_record, opening_type);
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

    // 定石の勝率を更新（使用した定石がある場合）
    if (g_current_game.opening_game_id >= 0) {
        int win = (result == 1) ? 1 : 0;
        int updated = update_opening_win_rate(g_current_game.opening_game_id, win);
        if (updated > 0) {
            printf("[Learning] Updated win rate for opening game %d (%d moves)\n",
                   g_current_game.opening_game_id, updated);

            // 定石データを保存
            save_opening_book("opening_book.csv");
        }
    }

    // パターン学習を常に適用して調整係数を更新（保存前に最新化）
    if (g_stats.total_games >= MIN_GAMES_FOR_LEARNING) {
        printf("[Learning] Applying pattern learning before saving...\n");
        apply_pattern_learning();
    }

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
        printf("[Learning] DEBUG_LEVEL=1: Loaded Learning Data from .dat file\n");
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
        printf("[Learning] ----------------------------------------\n");
        printf("[Learning]   Pattern Learning Data (from .dat):\n");
        for (int i = 0; i < 8; i++) {
            const char *pattern_names[] = {
                "OPEN_THREE", "BROKEN_THREE", "THREE", "OPEN_TWO",
                "TWO", "CENTER", "NEAR_ENEMY", "CONNECTION"
            };
            int total = g_pattern_learning[i].wins + g_pattern_learning[i].losses;
            double win_rate = total > 0 ? (g_pattern_learning[i].wins * 100.0 / total) : 0.0;
            printf("[Learning]     [%d] %-15s: wins=%d, losses=%d, total=%d, win_rate=%.2f%%, factor=%.3f\n",
                   i, pattern_names[i],
                   g_pattern_learning[i].wins, g_pattern_learning[i].losses, total,
                   win_rate, g_pattern_learning[i].adjustment_factor);
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

// ========================================
// AIの色を設定
// ========================================
void set_ai_color(int color) {
    if (!g_learning_initialized) {
        init_learning();
    }
    if (g_current_game.ai_color == STONE_SPACE) {
        g_current_game.ai_color = color;
        printf("[Learning] AI color set to: %s\n",
               color == STONE_BLACK ? "BLACK" : "WHITE");
    }
}
