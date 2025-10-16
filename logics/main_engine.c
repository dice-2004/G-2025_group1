
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "main_engine.h"
#include "../gomoku_ai.h"
#include "../config.h"
#include "evaluation.h"
#include "transposition.h"

// 定数定義
#define INF 999999
#define MAX_DEPTH 10  // 最大探索深度

// デバッグ用グローバル変数
static int g_nodes_searched = 0;      // 探索ノード数
static int g_cutoffs = 0;             // 枝刈り回数
static int g_tt_hits_in_search = 0;   // 置換表ヒット数
static clock_t g_search_start_time;   // 探索開始時刻
static clock_t g_search_deadline;     // 絶対的な探索終了時刻
static int g_time_limit_exceeded = 0; // 時間制限超過フラグ
static int g_early_cutoff_threshold = 8000; // 早期カットオフ閾値（デフォルト: open-four相当）

// 前方宣言
static int alpha_beta(const int board[][BOARD_SIZE], int depth, int alpha, int beta,
                      int color, int max_color, uint64_t hash);
static int is_game_over(const int board[][BOARD_SIZE]);
static int check_five(const int board[][BOARD_SIZE], int color);

// ========================================
// ゲーム終了判定
// ========================================

// 五連判定
static int check_five(const int board[][BOARD_SIZE], int color) {
    // 4方向をチェック
    int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (board[y][x] != color) continue;

            for (int d = 0; d < 4; d++) {
                int dx = directions[d][0];
                int dy = directions[d][1];
                int count = 1;

                // 正方向をカウント
                int nx = x + dx;
                int ny = y + dy;
                while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE &&
                       board[ny][nx] == color) {
                    count++;
                    nx += dx;
                    ny += dy;
                }

                // 逆方向をカウント
                nx = x - dx;
                ny = y - dy;
                while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE &&
                       board[ny][nx] == color) {
                    count++;
                    nx -= dx;
                    ny -= dy;
                }

                if (count >= 5) return 1;
            }
        }
    }
    return 0;
}

// ゲーム終了判定
static int is_game_over(const int board[][BOARD_SIZE]) {
    // 黒または白が五連を作ったか
    if (check_five(board, STONE_BLACK)) return 1;
    if (check_five(board, STONE_WHITE)) return 1;

    // 盤面が埋まったか
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (board[y][x] == STONE_SPACE) return 0;
        }
    }
    return 1;
}

// ========================================
// Alpha-Beta探索
// ========================================

// 再帰的なAlpha-Beta探索（置換表対応）
static int alpha_beta(const int board[][BOARD_SIZE], int depth, int alpha, int beta,
                      int color, int max_color, uint64_t hash) {
    g_nodes_searched++;  // デバッグ: ノード数カウント

    // 時間制限の厳格なチェック（探索途中でも必ず停止）
    if (g_search_deadline > 0) {
        clock_t current = clock();
        if (current >= g_search_deadline) {
            g_time_limit_exceeded = 1;
            return 0;  // 時間切れ、評価値は不定だが即座に返す
        }
    }

    // 終了条件: 深さ0 or ゲーム終了
    if (depth == 0 || is_game_over(board)) {
        int eval = evaluate_board(board, max_color);
        if (g_config.debug_level >= 3) {
            printf("  [Leaf] depth=%d eval=%d\n", depth, eval);
        }
        return eval;
    }

    // 置換表を検索
    int tt_score, tt_x, tt_y;
    if (probe_tt(hash, depth, alpha, beta, &tt_score, &tt_x, &tt_y)) {
        g_tt_hits_in_search++;  // デバッグ: TTヒット数カウント
        if (g_config.debug_level >= 3) {
            printf("  [TT Hit] depth=%d score=%d move=(%d,%d)\n", depth, tt_score, tt_x, tt_y);
        }
        return tt_score;
    }

    // 合法手を生成
    Move moves[BOARD_SIZE * BOARD_SIZE];
    int move_count = generate_legal_moves(board, color, moves, BOARD_SIZE * BOARD_SIZE);

    if (move_count == 0) {
        // 合法手がない場合は評価値を返す
        return evaluate_board(board, max_color);
    }

    // 相手の色
    int opponent = (color == STONE_BLACK) ? STONE_WHITE : STONE_BLACK;

    int best_x = moves[0].x;
    int best_y = moves[0].y;

    // Maxノード（自分の手番）
    if (color == max_color) {
        int max_eval = -INF;

        for (int i = 0; i < move_count; i++) {
            // ループ内での時間制限チェック
            if (g_search_deadline > 0) {
                clock_t current = clock();
                if (current >= g_search_deadline) {
                    g_time_limit_exceeded = 1;
                    if (g_config.debug_level >= 3) {
                        printf("  [Time Limit] Exceeded in Max loop at move %d/%d\n", i+1, move_count);
                    }
                    return max_eval;  // 現在の最良値を返す
                }
            }

            // 手を打つ
            int temp_board[BOARD_SIZE][BOARD_SIZE];
            memcpy(temp_board, board, sizeof(temp_board));
            temp_board[moves[i].y][moves[i].x] = color;

            // ハッシュ値を更新
            uint64_t new_hash = update_zobrist_hash(hash, moves[i].x, moves[i].y, color);

            // 再帰呼び出し
            int eval = alpha_beta(temp_board, depth - 1, alpha, beta, opponent, max_color, new_hash);

            // 時間切れフラグがセットされていたら即座に返す
            if (g_time_limit_exceeded) {
                return max_eval;
            }

            if (eval > max_eval) {
                max_eval = eval;
                best_x = moves[i].x;
                best_y = moves[i].y;
            }
            alpha = (alpha > eval) ? alpha : eval;

            // Beta枝刈り
            if (beta <= alpha) {
                g_cutoffs++;  // デバッグ: 枝刈りカウント
                if (g_config.debug_level >= 3) {
                    printf("  [Beta Cutoff] at move (%d,%d) i=%d/%d\n",
                           moves[i].x, moves[i].y, i+1, move_count);
                }
                break;
            }
        }

        // 置換表に保存
        int tt_type = (max_eval <= alpha) ? TT_ALPHA :
                      (max_eval >= beta) ? TT_BETA : TT_EXACT;
        store_tt(hash, depth, max_eval, tt_type, best_x, best_y);

        return max_eval;
    }
    // Minノード（相手の手番）
    else {
        int min_eval = INF;

        for (int i = 0; i < move_count; i++) {
            // ループ内での時間制限チェック
            if (g_search_deadline > 0) {
                clock_t current = clock();
                if (current >= g_search_deadline) {
                    g_time_limit_exceeded = 1;
                    if (g_config.debug_level >= 3) {
                        printf("  [Time Limit] Exceeded in Min loop at move %d/%d\n", i+1, move_count);
                    }
                    return min_eval;  // 現在の最良値を返す
                }
            }

            // 手を打つ
            int temp_board[BOARD_SIZE][BOARD_SIZE];
            memcpy(temp_board, board, sizeof(temp_board));
            temp_board[moves[i].y][moves[i].x] = color;

            // ハッシュ値を更新
            uint64_t new_hash = update_zobrist_hash(hash, moves[i].x, moves[i].y, color);

            // 再帰呼び出し
            int eval = alpha_beta(temp_board, depth - 1, alpha, beta, opponent, max_color, new_hash);

            // 時間切れフラグがセットされていたら即座に返す
            if (g_time_limit_exceeded) {
                return min_eval;
            }

            if (eval < min_eval) {
                min_eval = eval;
                best_x = moves[i].x;
                best_y = moves[i].y;
            }
            beta = (beta < eval) ? beta : eval;

            // Alpha枝刈り
            if (beta <= alpha) {
                g_cutoffs++;  // デバッグ: 枝刈りカウント
                if (g_config.debug_level >= 3) {
                    printf("  [Alpha Cutoff] at move (%d,%d) i=%d/%d\n",
                           moves[i].x, moves[i].y, i+1, move_count);
                }
                break;
            }
        }

        // 置換表に保存
        int tt_type = (min_eval <= alpha) ? TT_ALPHA :
                      (min_eval >= beta) ? TT_BETA : TT_EXACT;
        store_tt(hash, depth, min_eval, tt_type, best_x, best_y);

        return min_eval;
    }
}

// ========================================
// Alpha-Beta探索のエントリポイント
// ========================================

// 手の評価値を保存する構造体
typedef struct {
    int x, y;
    int score;
} ScoredMove;

// スコア比較関数（降順ソート用）
static int compare_scored_moves(const void *a, const void *b) {
    const ScoredMove *ma = (const ScoredMove *)a;
    const ScoredMove *mb = (const ScoredMove *)b;
    return mb->score - ma->score;  // 降順
}

int alpha_beta_search(const int board[][BOARD_SIZE], int color, int depth,
                      int *best_x, int *best_y) {
    // デバッグ統計をリセット
    g_nodes_searched = 0;
    g_cutoffs = 0;
    g_tt_hits_in_search = 0;
    g_search_start_time = clock();

    Move moves[BOARD_SIZE * BOARD_SIZE];
    int move_count = generate_legal_moves(board, color, moves, BOARD_SIZE * BOARD_SIZE);

    if (move_count == 0) {
        *best_x = BOARD_SIZE / 2;
        *best_y = BOARD_SIZE / 2;
        return 0;
    }

    int best_score = -INF;
    int opponent = (color == STONE_BLACK) ? STONE_WHITE : STONE_BLACK;

    // 現在の局面のハッシュ値を計算
    uint64_t hash = compute_zobrist_hash(board);

    if (g_config.debug_level >= 1) {
        printf("[Alpha-Beta] Searching %d moves at depth %d...\n", move_count, depth);
    }

    // 各手に評価値を割り当ててソート
    ScoredMove scored_moves[BOARD_SIZE * BOARD_SIZE];
    for (int i = 0; i < move_count; i++) {
        scored_moves[i].x = moves[i].x;
        scored_moves[i].y = moves[i].y;
        scored_moves[i].score = 0;  // 初期値
    }

    for (int i = 0; i < move_count; i++) {
        // ルート探索での時間制限チェック
        if (g_search_deadline > 0) {
            clock_t current = clock();
            if (current >= g_search_deadline) {
                g_time_limit_exceeded = 1;
                if (g_config.debug_level >= 2) {
                    printf("[Root] Time limit exceeded at move %d/%d\n", i+1, move_count);
                }
                break;  // これまでの最良手を使用
            }
        }

        // 手を打つ
        int temp_board[BOARD_SIZE][BOARD_SIZE];
        memcpy(temp_board, board, sizeof(temp_board));
        temp_board[scored_moves[i].y][scored_moves[i].x] = color;

        // ハッシュ値を更新
        uint64_t new_hash = update_zobrist_hash(hash, scored_moves[i].x, scored_moves[i].y, color);

        // Alpha-Beta探索
        int score = alpha_beta(temp_board, depth - 1, -INF, INF, opponent, color, new_hash);
        scored_moves[i].score = score;

        // 時間切れフラグがセットされていたら終了
        if (g_time_limit_exceeded) {
            if (g_config.debug_level >= 2) {
                printf("[Root] Time exceeded during search, stopping at move %d/%d\n", i+1, move_count);
            }
            break;
        }

        if (g_config.debug_level >= 3) {
            printf("[Root] Move %d/%d: (%d,%d) score=%d\n",
                   i+1, move_count, scored_moves[i].x, scored_moves[i].y, score);
        }

        if (score > best_score) {
            best_score = score;
        }
    }

    // 次の深さのために手をソート（評価値の高い順）
    qsort(scored_moves, move_count, sizeof(ScoredMove), compare_scored_moves);

    if (g_config.debug_level >= 2) {
        printf("[Alpha-Beta] Moves sorted by score (top 5):\n");
        for (int i = 0; i < move_count && i < 5; i++) {
            printf("  %d. (%d,%d) score=%d\n", i+1,
                   scored_moves[i].x, scored_moves[i].y, scored_moves[i].score);
        }
    }

    *best_x = scored_moves[0].x;  // ソート後の最良手
    *best_y = scored_moves[0].y;

    // デバッグ統計を出力
    clock_t end_time = clock();
    double elapsed = (double)(end_time - g_search_start_time) / CLOCKS_PER_SEC;

    if (g_config.debug_level >= 1) {
        printf("[Alpha-Beta] Best move: (%d, %d) with score %d\n", *best_x, *best_y, best_score);
    }

    if (g_config.debug_level >= 2) {
        printf("[Debug Stats]\n");
        printf("  Nodes searched: %d\n", g_nodes_searched);
        printf("  Cutoffs: %d (%.1f%%)\n", g_cutoffs,
               (move_count > 0) ? (100.0 * g_cutoffs / g_nodes_searched) : 0.0);
        printf("  TT hits: %d (%.1f%%)\n", g_tt_hits_in_search,
               (g_nodes_searched > 0) ? (100.0 * g_tt_hits_in_search / g_nodes_searched) : 0.0);
        printf("  Time: %.3f sec\n", elapsed);
        printf("  Nodes/sec: %.0f\n", (elapsed > 0) ? (g_nodes_searched / elapsed) : 0);
        print_tt_stats();
    }

    return best_score;
}

// ========================================
// 早期カットオフ閾値の取得・設定（学習用）
// ========================================

int get_early_cutoff_threshold(void) {
    return g_early_cutoff_threshold;
}

void set_early_cutoff_threshold(int threshold) {
    // 範囲チェック：3000～15000の範囲に制限
    if (threshold < 3000) threshold = 3000;
    if (threshold > 15000) threshold = 15000;

    g_early_cutoff_threshold = threshold;

    printf("[Engine] Early cutoff threshold set to %d\n", g_early_cutoff_threshold);
}

// ========================================
// 反復深化探索
// ========================================

int iterative_deepening(const int board[][BOARD_SIZE], int color, int time_limit_ms,
                        int *best_x, int *best_y) {
    // 前の深度の結果を保持（時間切れ時に返す）
    int prev_x = -1, prev_y = -1, prev_score = 0;
    int best_score = 0;
    int depth = 1;
    int completed_depth = 0;  // 完了した最大深度
    clock_t start_time = clock();

    // 時間制限に基づく最大深度の決定
    int max_depth = g_config.search_depth;
    if (time_limit_ms > 0) {
        // 時間制限がある場合は動的に調整
        if (time_limit_ms < 1000) max_depth = 3;
        else if (time_limit_ms < 3000) max_depth = 4;
        else if (time_limit_ms < 10000) max_depth = 6;
        else max_depth = MAX_DEPTH;
    }

    // 閾値の取得（config.txtから読み込み、0.0～1.0）
    double threshold = g_config.id_time_threshold;
    if (threshold < 0.0) threshold = 0.0;
    if (threshold > 1.0) threshold = 1.0;

    // 厳格な時間制限の取得（0の場合はtime_limit_msと同じ）
    int strict_time_limit = g_config.strict_time_limit_ms;
    if (strict_time_limit == 0) {
        strict_time_limit = time_limit_ms;
    }

    if (g_config.debug_level >= 1) {
        printf("\n========================================\n");
        printf("[Iterative Deepening] Starting search\n");
        printf("  Max depth: %d\n", max_depth);
        printf("  Time limit: %d ms\n", time_limit_ms);
        printf("  Strict time limit: %d ms", strict_time_limit);
        if (g_config.strict_time_limit_ms == 0) {
            printf(" (same as time limit)\n");
        } else {
            printf(" (configured separately)\n");
        }
        printf("  Time threshold: %.0f%%\n", threshold * 100);
        printf("  Early cutoff threshold: %d\n", g_early_cutoff_threshold);
        printf("========================================\n\n");
    }

    // 深さ1から徐々に深くする
    for (depth = 1; depth <= max_depth; depth++) {
        // 深度探索開始前に時間チェック
        clock_t current_time = clock();
        double elapsed = (double)(current_time - start_time) / CLOCKS_PER_SEC * 1000;

        // 【重要】厳格な時間制限を先にチェック（絶対に超えてはいけない）
        if (strict_time_limit > 0 && elapsed >= strict_time_limit) {
            if (g_config.debug_level >= 1) {
                printf("[ID] Strict time limit reached before depth %d (%.0fms >= %dms strict).\n",
                       depth, elapsed, strict_time_limit);
                printf("[ID] Returning result from depth %d.\n", completed_depth);
            }
            break;
        }

        // 通常の閾値チェック（次の深度に進むかの判定）
        if (time_limit_ms > 0 && elapsed > time_limit_ms * threshold) {
            if (g_config.debug_level >= 1) {
                printf("[ID] Time threshold reached (%.0fms / %dms = %.1f%%), stopping before depth %d.\n",
                       elapsed, time_limit_ms, (elapsed / time_limit_ms) * 100, depth);
                printf("[ID] Returning result from depth %d.\n", completed_depth);
            }
            break;
        }

        // 厳格な時間制限を設定（この深さの探索中に絶対に超えてはいけない時間）
        g_time_limit_exceeded = 0;
        if (strict_time_limit > 0) {
            // 厳格な時間制限を使用してデッドラインを設定
            // これは元のtime_limit_msとは独立した、絶対に守らなければならない制限
            g_search_deadline = start_time + (clock_t)((strict_time_limit * CLOCKS_PER_SEC) / 1000);

            if (g_config.debug_level >= 3) {
                double strict_remaining = strict_time_limit - elapsed;
                printf("[ID] Strict deadline set: %.0f ms remaining (of %d ms strict limit)\n",
                       strict_remaining, strict_time_limit);
            }
        } else {
            g_search_deadline = 0;  // 時間制限なし
        }

        clock_t depth_start = clock();

        int temp_x, temp_y;
        int score = alpha_beta_search(board, color, depth, &temp_x, &temp_y);

        clock_t depth_end = clock();
        double depth_time = (double)(depth_end - depth_start) / CLOCKS_PER_SEC;

        // 探索完了後に時間チェック
        elapsed = (double)(depth_end - start_time) / CLOCKS_PER_SEC * 1000;

        // 厳格な時間制限を超えた場合は、この深度の結果を破棄して前の深度の結果を返す
        // これは通常の時間制限とは別に、必ず守られるべき制限
        if (strict_time_limit > 0 && elapsed > strict_time_limit) {
            if (g_config.debug_level >= 1) {
                printf("[ID] Strict time limit exceeded during depth %d search (%.0fms / %dms strict).\n",
                       depth, elapsed, strict_time_limit);
                printf("[ID] Discarding depth %d result, returning depth %d result: (%d, %d) score=%d\n",
                       depth, completed_depth, prev_x, prev_y, prev_score);
            }
            // 前の深度の結果を返す
            *best_x = prev_x;
            *best_y = prev_y;
            best_score = prev_score;
            break;
        }

        // 時間制限超過フラグがセットされている場合、この深さの結果を破棄
        if (g_time_limit_exceeded) {
            if (g_config.debug_level >= 1) {
                printf("[ID] Strict time limit exceeded during depth %d search (detected by flag).\n", depth);
                printf("[ID] Discarding depth %d result, returning depth %d result: (%d, %d) score=%d\n",
                       depth, completed_depth, prev_x, prev_y, prev_score);
            }
            *best_x = prev_x;
            *best_y = prev_y;
            best_score = prev_score;
            break;
        }

        // 探索成功 - 前の結果を保存してから更新
        prev_x = *best_x;
        prev_y = *best_y;
        prev_score = best_score;

        *best_x = temp_x;
        *best_y = temp_y;
        best_score = score;
        completed_depth = depth;

        if (g_config.debug_level >= 1) {
            printf("[ID] Depth %d completed: (%d, %d) score=%d time=%.3fs (elapsed: %.0fms / %dms = %.1f%%)\n",
                   depth, *best_x, *best_y, score, depth_time, elapsed, time_limit_ms,
                   time_limit_ms > 0 ? (elapsed / time_limit_ms) * 100 : 0);
        }

        // 勝ちが見つかったら終了（最優先）
        if (score > SCORE_FIVE / 2) {
            if (g_config.debug_level >= 1) {
                printf("[ID] Winning move found! (score=%d)\n", score);
            }
            break;
        }

        // 早期カットオフ: 十分に高い評価値 + 時間的余裕がない場合のみ
        if (g_early_cutoff_threshold > 0 && score >= g_early_cutoff_threshold) {
            double time_usage = (time_limit_ms > 0) ? (elapsed / time_limit_ms) : 0.0;
            double time_threshold = g_config.early_cutoff_time_threshold;

            // 時間を閾値以上使っている場合のみカットオフ
            if (time_usage >= time_threshold) {
                if (g_config.debug_level >= 1) {
                    printf("[ID] Early cutoff triggered! score=%d >= threshold=%d (time usage: %.1f%% >= %.1f%%)\n",
                           score, g_early_cutoff_threshold, time_usage * 100, time_threshold * 100);
                    printf("[ID] Returning without deeper search.\n");
                }
                break;
            } else {
                if (g_config.debug_level >= 2) {
                    printf("[ID] High score (%d >= %d) but time remaining (%.1f%% < %.1f%%), continuing search.\n",
                           score, g_early_cutoff_threshold, time_usage * 100, time_threshold * 100);
                }
            }
        }

        // 負けが確定したら終了
        if (score < -SCORE_FIVE / 2) {
            if (g_config.debug_level >= 1) {
                printf("[ID] Losing position detected. (score=%d)\n", score);
            }
            break;
        }

        // 次の深度の予想時間が制限を超えそうなら終了
        if (time_limit_ms > 0 && depth_time > 0) {
            double predicted_next = depth_time * 3.0;  // 次の深度は約3倍と仮定
            double remaining = time_limit_ms - elapsed;
            if (predicted_next > remaining) {
                if (g_config.debug_level >= 2) {
                    printf("[ID] Predicted timeout for depth %d (%.0fms > %.0fms remaining), stopping.\n",
                           depth + 1, predicted_next, remaining);
                }
                break;
            }
        }
    }

    double total_time = (double)(clock() - start_time) / CLOCKS_PER_SEC;

    if (g_config.debug_level >= 1) {
        printf("\n========================================\n");
        printf("[ID] Search completed\n");
        printf("  Completed depth: %d\n", completed_depth);
        printf("  Best move: (%d, %d)\n", *best_x, *best_y);
        printf("  Best score: %d\n", best_score);
        printf("  Total time: %.3f sec\n", total_time);
        printf("========================================\n\n");
    }

    return best_score;
}
