#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "opening_book.h"

static OpeningMove *g_book = NULL;
static int g_book_size = 0;
static int g_selected_game_id = -1;  // 選択されたゲームID
static int g_last_move_count = -1;   // 前回の手数

// コメントからゲームIDを抽出
// "Learned from win (game 123)" → 123
// "初手は中央" → -1 (ゲームIDなし)
static int extract_game_id(const char *comment) {
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

    // 最大100手分の定石を読み込み
    g_book = (OpeningMove*)malloc(sizeof(OpeningMove) * 100);
    if (!g_book) {
        fclose(fp);
        return 0;
    }

    char line[256];
    int count = 0;

    while (fgets(line, sizeof(line), fp) && count < 100) {
        // コメント行と空行をスキップ
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        int move_num, x, y;
        char comment[128] = "";

        // CSV解析
        if (sscanf(line, "%d,%d,%d,%[^\n]", &move_num, &x, &y, comment) >= 3) {
            g_book[count].move_number = move_num;
            g_book[count].x = x;
            g_book[count].y = y;
            // strncpyの警告を回避: 明示的にnull終端を保証
            strncpy(g_book[count].comment, comment, sizeof(g_book[count].comment) - 1);
            g_book[count].comment[sizeof(g_book[count].comment) - 1] = '\0';  // null終端を明示的に設定

            // ゲームIDを抽出してキャッシュ
            g_book[count].game_id = extract_game_id(comment);

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
int find_opening_move(const int board[][BOARD_SIZE], int move_count, int *x, int *y) {
    if (g_book == NULL || g_book_size == 0) {
        return 0;
    }

    // ==============================
    // フェーズ1: ゲーム開始検出
    // ==============================
    if (move_count == 0) {
        g_selected_game_id = -1;
        g_last_move_count = -1;
        printf("[OpeningBook] New game started, reset game ID\n");
    }

    // ==============================
    // フェーズ2: 1手目でゲームID選択
    // ==============================
    if (move_count == 0 && g_selected_game_id == -1) {
        int game_ids[100];
        int game_id_count = 0;

        // 1手目の候補からゲームIDを収集
        for (int i = 0; i < g_book_size; i++) {
            if (g_book[i].move_number == 1 && g_book[i].game_id >= 0) {
                int gid = g_book[i].game_id;

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
        }

        // ランダムに1つ選択
        if (game_id_count > 0) {
            g_selected_game_id = game_ids[rand() % game_id_count];
            printf("[OpeningBook] Selected game sequence: %d (from %d games)\n",
                   g_selected_game_id, game_id_count);
        }
    }

    // ==============================
    // フェーズ3: 選択されたゲームIDの定石を検索
    // ==============================
    if (g_selected_game_id >= 0) {
        for (int i = 0; i < g_book_size; i++) {
            if (g_book[i].move_number == move_count + 1 &&
                g_book[i].game_id == g_selected_game_id) {

                int ox = g_book[i].x;
                int oy = g_book[i].y;

                if (ox >= 0 && ox < BOARD_SIZE && oy >= 0 && oy < BOARD_SIZE) {
                    if (board[oy][ox] == STONE_SPACE) {
                        *x = ox;
                        *y = oy;
                        printf("[OpeningBook] Move %d from game %d: (%d,%d) - %s\n",
                               move_count + 1, g_selected_game_id, *x, *y,
                               g_book[i].comment);
                        g_last_move_count = move_count;
                        return 1;
                    }
                }
            }
        }

        // 該当ゲームIDの定石がない場合
        printf("[OpeningBook] Move %d not found for game %d, trying other games\n",
               move_count + 1, g_selected_game_id);
    }

    // ==============================
    // フェーズ4: フォールバック（別のゲームIDを試す）
    // ==============================
    for (int i = 0; i < g_book_size; i++) {
        if (g_book[i].move_number == move_count + 1) {
            int ox = g_book[i].x;
            int oy = g_book[i].y;

            if (ox >= 0 && ox < BOARD_SIZE && oy >= 0 && oy < BOARD_SIZE) {
                if (board[oy][ox] == STONE_SPACE) {
                    *x = ox;
                    *y = oy;

                    if (g_book[i].game_id >= 0) {
                        g_selected_game_id = g_book[i].game_id;
                        printf("[OpeningBook] Switched to game %d for move %d: (%d,%d)\n",
                               g_selected_game_id, move_count + 1, *x, *y);
                    } else {
                        printf("[OpeningBook] Move %d: (%d,%d) - %s (no game ID)\n",
                               move_count + 1, *x, *y, g_book[i].comment);
                    }

                    g_last_move_count = move_count;
                    return 1;
                }
            }
        }
    }

    return 0;  // 定石なし
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
