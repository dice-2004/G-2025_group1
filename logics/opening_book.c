#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "opening_book.h"

static OpeningMove *g_book = NULL;
static int g_book_size = 0;

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
            count++;
        }
    }

    fclose(fp);
    g_book_size = count;

    printf("[OpeningBook] Loaded %d moves from %s\n", count, filename);
    return count;
}

// 現在の局面から定石手を検索
int find_opening_move(const int board[][BOARD_SIZE], int move_count, int *x, int *y) {
    if (g_book == NULL || g_book_size == 0) {
        return 0;
    }

    // 手数が合う定石を探す
    for (int i = 0; i < g_book_size; i++) {
        if (g_book[i].move_number == move_count + 1) {
            // 候補手が空きマスか確認
            int ox = g_book[i].x;
            int oy = g_book[i].y;

            if (ox >= 0 && ox < BOARD_SIZE && oy >= 0 && oy < BOARD_SIZE) {
                if (board[oy][ox] == STONE_SPACE) {
                    *x = ox;
                    *y = oy;
                    printf("[OpeningBook] Found move %d: (%d,%d) - %s\n",
                           move_count + 1, *x, *y, g_book[i].comment);
                    return 1;
                }
            }
        }
    }

    return 0;
}

// 定石の解放
void free_opening_book(void) {
    if (g_book != NULL) {
        free(g_book);
        g_book = NULL;
        g_book_size = 0;
    }
}
