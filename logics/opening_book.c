#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "opening_book.h"
#include "../config.h"

static OpeningMove *g_book = NULL;
static int g_book_size = 0;
static int g_selected_game_id = -1;  // 選択されたゲームID
static int g_last_move_count = -1;   // 前回の手数

// コメントからゲームIDを抽出
// "Learned from win (game 123)" → 123
// "初手は中央" → -1 (ゲームIDなし)
static __attribute__((unused)) int extract_game_id(const char *comment) {
    const char *marker = "game ";
    const char *pos = strstr(comment, marker);

    if (pos == NULL) {
        return -1;  // ゲームIDなし
    }

    pos += strlen(marker);
    return atoi(pos);
}

// ゲームID統計を表示
static void print_game_id_stats(void) {
    int game_ids[100];
    int game_id_count = 0;

    // 重複を除いてゲームIDを収集
    for (int i = 0; i < g_book_size; i++) {
        int gid = g_book[i].game_id;
        if (gid < 0) continue;

        // 重複チェック
        int found = 0;
        for (int j = 0; j < game_id_count; j++) {
            if (game_ids[j] == gid) {
                found = 1;
                break;
            }
        }

        if (!found) {
            game_ids[game_id_count++] = gid;
        }
    }

    printf("[OpeningBook] Found %d unique game sequences\n", game_id_count);

    if (game_id_count > 0 && game_id_count <= 10) {
        printf("[OpeningBook] Game IDs: ");
        for (int i = 0; i < game_id_count; i++) {
            printf("%d ", game_ids[i]);
        }
        printf("\n");
    }
}

// 序盤定石の読み込み
int load_opening_book(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("[OpeningBook] Cannot open %s\n", filename);
        return 0;
    }

    // 既存の定石を解放
    if (g_book != NULL) {
        free(g_book);
        g_book = NULL;
        g_book_size = 0;
    }

    // 最大200手分の定石を読み込み（11ゲーム × 10手 × 2パターン = 220）
    g_book = (OpeningMove*)malloc(sizeof(OpeningMove) * 250);
    if (!g_book) {
        fclose(fp);
        return 0;
    }

    char line[256];
    int count = 0;

    while (fgets(line, sizeof(line), fp) && count < 250) {
        // コメント行と空行をスキップ
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        int game_id, move_num, x, y;
        char color_str[16] = "";
        double win_rate = 0.5;
        char comment[128] = "";

        // 新フォーマット: game_id,move_number,player_color,x,y,win_rate,comment
        int parsed = sscanf(line, "%d,%d,%[^,],%d,%d,%lf,%[^\n]",
                           &game_id, &move_num, color_str, &x, &y, &win_rate, comment);

        if (parsed >= 5) {
            g_book[count].game_id = game_id;
            g_book[count].move_number = move_num;

            // player_colorの解析
            if (strcmp(color_str, "BLACK") == 0) {
                g_book[count].player_color = STONE_BLACK;
            } else if (strcmp(color_str, "WHITE") == 0) {
                g_book[count].player_color = STONE_WHITE;
            } else {
                printf("[OpeningBook] Warning: Unknown color '%s' at line, skipping\n", color_str);
                continue;
            }

            g_book[count].x = x;
            g_book[count].y = y;
            g_book[count].win_rate = (parsed >= 6) ? win_rate : 0.5;

            // strncpyの警告を回避: 明示的にnull終端を保証
            if (parsed >= 7) {
                strncpy(g_book[count].comment, comment, sizeof(g_book[count].comment) - 1);
            } else {
                g_book[count].comment[0] = '\0';
            }
            g_book[count].comment[sizeof(g_book[count].comment) - 1] = '\0';

            count++;
        }
    }

    fclose(fp);
    g_book_size = count;

    printf("[OpeningBook] Loaded %d moves from %s\n", count, filename);

    // ゲームID統計を表示
    print_game_id_stats();

    return count;
}

// 現在の局面から定石手を検索（ゲーム単位選択）
// move_count: ゲーム全体の手数（0-indexed → 1-indexed変換が必要）
// ai_color: AIの色（STONE_BLACK or STONE_WHITE）
int find_opening_move(const int board[][BOARD_SIZE], int move_count, int ai_color, int *x, int *y) {
    if (g_book == NULL || g_book_size == 0) {
        return 0;
    }

    // move_countは0-indexedなので、1-indexedに変換
    int current_move_number = move_count + 1;

    // 定石は10手目まで
    if (current_move_number > 10) {
        return 0;
    }

    // ==============================
    // フェーズ1: ゲーム開始検出（move_count減少時）
    // ==============================
    if (move_count < g_last_move_count) {
        // 新しいゲームが始まった（手数が減った）
        g_selected_game_id = -1;
        g_last_move_count = -1;
        printf("[OpeningBook] New game detected, reset game ID\n");
    }

    // ==============================
    // フェーズ2: ゲームID選択（AIの最初の手番でまだ選ばれていない場合）
    // ==============================
    // 黒番AIなら1手目、白番AIなら2手目で選択
    int ai_first_move = (ai_color == STONE_BLACK) ? 1 : 2;

    if (g_selected_game_id == -1 && current_move_number == ai_first_move) {
        int game_ids[100];
        double win_rates[100];
        int game_id_count = 0;

        // AIの最初の手番の候補からゲームIDと勝率を収集
        printf("[OpeningBook] Selecting opening for AI color %s (first move = %d)\n",
               ai_color == STONE_BLACK ? "BLACK" : "WHITE", ai_first_move);

        for (int i = 0; i < g_book_size; i++) {
            if (g_book[i].move_number == ai_first_move &&
                g_book[i].player_color == ai_color &&
                g_book[i].game_id >= 0) {
                int gid = g_book[i].game_id;

                // game_id範囲によるフィルタリング
                // 黒番AI(先手)用: game_id 1-49(手動), 101-149(プロ), 200-599(学習)
                // 白番AI(後手)用: game_id 51-99(手動), 151-199(プロ), 600-999(学習)
                int is_valid_for_ai = 0;
                if (ai_color == STONE_BLACK) {
                    // 黒番AI用の定石範囲
                    if ((gid >= 1 && gid <= 49) ||      // 手動（黒）
                        (gid >= 101 && gid <= 149) ||   // プロ（黒）
                        (gid >= 200 && gid <= 599)) {   // 学習（黒）
                        is_valid_for_ai = 1;
                    }
                } else {  // ai_color == STONE_WHITE
                    // 白番AI用の定石範囲
                    if ((gid >= 51 && gid <= 99) ||     // 手動（白）
                        (gid >= 151 && gid <= 199) ||   // プロ（白）
                        (gid >= 600 && gid <= 999)) {   // 学習（白）
                        is_valid_for_ai = 1;
                    }
                }

                if (!is_valid_for_ai) {
                    continue;  // この定石はAIの色に合わない
                }

                // 重複チェック
                int found = 0;
                for (int j = 0; j < game_id_count; j++) {
                    if (game_ids[j] == gid) {
                        found = 1;
                        break;
                    }
                }

                if (!found) {
                    game_ids[game_id_count] = gid;
                    win_rates[game_id_count] = g_book[i].win_rate;
                    game_id_count++;
                }
            }
        }

        if (game_id_count > 0) {
            // デバッグ: 候補ゲームIDをリスト表示
            printf("[OpeningBook] Available games for %s: ",
                   ai_color == STONE_BLACK ? "BLACK" : "WHITE");
            for (int i = 0; i < game_id_count; i++) {
                printf("%d", game_ids[i]);
                if (i < game_id_count - 1) printf(", ");
            }
            printf("\n");

            // 学習モード: ランダム選択
            if (g_config.learning_mode == LEARNING_MODE_PRACTICE) {
                g_selected_game_id = game_ids[rand() % game_id_count];
                printf("[OpeningBook] PRACTICE mode: Random game %d selected\n",
                       g_selected_game_id);
            }
            // 本番モード: 勝率最高のゲームを選択
            else {
                int best_idx = 0;
                double best_win_rate = win_rates[0];

                for (int i = 1; i < game_id_count; i++) {
                    if (win_rates[i] > best_win_rate) {
                        best_win_rate = win_rates[i];
                        best_idx = i;
                    }
                }

                g_selected_game_id = game_ids[best_idx];
                printf("[OpeningBook] PRODUCTION mode: Best game %d selected (win_rate=%.3f)\n",
                       g_selected_game_id, best_win_rate);
            }
        } else {
            printf("[OpeningBook] ERROR: No opening available for AI color %s at move %d\n",
                   ai_color == STONE_BLACK ? "BLACK" : "WHITE", ai_first_move);
            return 0;
        }
    }

    // ==============================
    // フェーズ3: 選択されたゲームIDからAIの手を検索
    // ==============================
    if (g_selected_game_id >= 0) {
        for (int i = 0; i < g_book_size; i++) {
            if (g_book[i].move_number == current_move_number &&
                g_book[i].player_color == ai_color &&
                g_book[i].game_id == g_selected_game_id) {

                int ox = g_book[i].x;
                int oy = g_book[i].y;

                if (ox >= 0 && ox < BOARD_SIZE && oy >= 0 && oy < BOARD_SIZE) {
                    if (board[oy][ox] == STONE_SPACE) {
                        *x = ox;
                        *y = oy;
                        printf("[OpeningBook] Move %d (AI=%s) from game %d: (%d,%d) - %s\n",
                               current_move_number,
                               ai_color == STONE_BLACK ? "BLACK" : "WHITE",
                               g_selected_game_id, *x, *y,
                               g_book[i].comment);
                        g_last_move_count = move_count;
                        return 1;
                    } else {
                        printf("[OpeningBook] Warning: Opening position (%d,%d) already occupied!\n", ox, oy);
                    }
                }
            }
        }

        // 該当ゲームIDの定石がない場合
        printf("[OpeningBook] Move %d not found for game %d (AI=%s), abandoning opening\n",
               current_move_number, g_selected_game_id,
               ai_color == STONE_BLACK ? "BLACK" : "WHITE");
        g_selected_game_id = -1;  // 定石放棄
    }

    return 0;  // 定石なし
}

// 相手の応手が定石通りか検証
// move_count: ゲーム全体の手数（0-indexed）
// ai_color: AIの色
// opp_x, opp_y: 相手が打った座標
// 戻り値: 1=定石通り、0=定石から外れた
int validate_opponent_move(const int board[][BOARD_SIZE], int move_count, int ai_color, int opp_x, int opp_y) {
    if (g_book == NULL || g_book_size == 0) {
        return 0;
    }

    // 定石が選択されていない場合は検証不要
    if (g_selected_game_id < 0) {
        return 1;  // 定石を使っていないので常にOK
    }

    int current_move_number = move_count + 1;

    // 10手を超えたら定石終了
    if (current_move_number > 10) {
        return 1;
    }

    // 相手の色
    int opponent_color = (ai_color == STONE_BLACK) ? STONE_WHITE : STONE_BLACK;

    // 選択された定石の中から、この手番で相手が打つべき手を検索
    for (int i = 0; i < g_book_size; i++) {
        if (g_book[i].game_id == g_selected_game_id &&
            g_book[i].move_number == current_move_number &&
            g_book[i].player_color == opponent_color) {

            // 定石通りの手か確認
            if (g_book[i].x == opp_x && g_book[i].y == opp_y) {
                printf("[OpeningBook] Opponent move %d matches opening: (%d,%d) - %s\n",
                       current_move_number, opp_x, opp_y, g_book[i].comment);
                return 1;  // 定石通り
            } else {
                printf("[OpeningBook] Opponent deviated from opening at move %d!\n", current_move_number);
                printf("[OpeningBook]   Expected: (%d,%d), Got: (%d,%d)\n",
                       g_book[i].x, g_book[i].y, opp_x, opp_y);
                printf("[OpeningBook] Abandoning opening book, switching to TSS/Alpha-Beta\n");
                g_selected_game_id = -1;  // 定石放棄
                return 0;  // 定石から外れた
            }
        }
    }

    // この手番で相手の手が定石にない（定石が途中で終わった）
    printf("[OpeningBook] No opponent move found at move %d for game %d\n",
           current_move_number, g_selected_game_id);
    return 1;  // 定石が終了しただけなのでエラーではない
}

// 定石の解放
void free_opening_book(void) {
    if (g_book != NULL) {
        free(g_book);
        g_book = NULL;
        g_book_size = 0;
    }
    g_selected_game_id = -1;
    g_last_move_count = -1;
}

// 定石の勝率を更新（学習用）
// game_id: 更新対象のゲームID
// win: 1=勝利、0=敗北
// 戻り値: 更新した手数
int update_opening_win_rate(int game_id, int win) {
    if (g_book == NULL || game_id < 0) {
        return 0;
    }

    int updated_count = 0;

    // 該当ゲームIDのすべての手を更新
    for (int i = 0; i < g_book_size; i++) {
        if (g_book[i].game_id == game_id) {
            // 移動平均による勝率更新（学習率0.1）
            double learning_rate = 0.1;
            double target = win ? 1.0 : 0.0;
            g_book[i].win_rate = g_book[i].win_rate * (1.0 - learning_rate) + target * learning_rate;

            updated_count++;
        }
    }

    if (updated_count > 0 && g_config.debug_level >= 1) {
        printf("[OpeningBook] Updated win_rate for game %d: %d moves, result=%s\n",
               game_id, updated_count, win ? "WIN" : "LOSS");
    }

    return updated_count;
}

// 定石の勝率をCSVに保存
int save_opening_book(const char *filename) {
    if (g_book == NULL || g_book_size == 0) {
        printf("[OpeningBook] No data to save\n");
        return 0;
    }

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("[OpeningBook] Cannot open %s for writing\n", filename);
        return 0;
    }

    // ヘッダー出力
    fprintf(fp, "# Gomoku Opening Book v3.0\n");
    fprintf(fp, "# Format: move_number,x,y,win_rate,comment\n");
    fprintf(fp, "# 中央を(7,7)として座標を指定（15x15盤）\n");
    fprintf(fp, "# win_rate: 0.0～1.0（0.5=初期値、学習で更新される）\n");
    fprintf(fp, "#\n");
    fprintf(fp, "# ========================================\n");
    fprintf(fp, "# ゲームID規則:\n");
    fprintf(fp, "#   game 1～999: 学習定石（自動生成）\n");
    fprintf(fp, "#   game 101～199: 手動定石（基本パターン）\n");
    fprintf(fp, "#   game 201～299: プロの定石（標準パターン）\n");
    fprintf(fp, "# ========================================\n\n");

    // ゲームIDごとに整理して出力
    int current_game_id = -999;
    for (int i = 0; i < g_book_size; i++) {
        // 新しいゲームIDの場合、コメント出力
        if (g_book[i].game_id != current_game_id) {
            current_game_id = g_book[i].game_id;
            if (current_game_id >= 0) {
                fprintf(fp, "\n# ========================================\n");
                fprintf(fp, "# Game %d\n", current_game_id);
                fprintf(fp, "# ========================================\n");
            }
        }

        // データ出力
        fprintf(fp, "%d,%d,%d,%.2f,%s\n",
                g_book[i].move_number,
                g_book[i].x,
                g_book[i].y,
                g_book[i].win_rate,
                g_book[i].comment);
    }

    fclose(fp);
    printf("[OpeningBook] Saved %d moves to %s\n", g_book_size, filename);
    return g_book_size;
}

// 現在選択されているゲームIDを取得
int get_selected_game_id(void) {
    return g_selected_game_id;
}
