#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "gomoku_ai.h"
#include "gomoku.h"

int check = 0;

#define GEMINI_API_KEY "AIzaSyCzULu8thpB1nicQFamc9nyuEPu6ZeRkLY"  // ← あなたのAPIキーをここに
#define GEMINI_MODEL "gemini-2.5-flash"


// Geminiからの応答を受け取るバッファ
struct MemoryStruct {
    char *memory;
    size_t size;
};

// --- JSONエスケープ用関数 ---
void escape_json(const char *src, char *dst, size_t maxlen) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 2 < maxlen; i++) {
        if (src[i] == '\"') {
            dst[j++] = '\\';
            dst[j++] = '\"';
        } else if (src[i] == '\\') {
            dst[j++] = '\\';
            dst[j++] = '\\';
        } else if (src[i] == '\n') {
            dst[j++] = '\\';
            dst[j++] = 'n';
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}


int checkWinner(int board[BOARD_SIZE][BOARD_SIZE]);

// 書き込みコールバック
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(ptr == NULL) return 0;  // メモリ不足

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

// === from gomoku.c ===
// 盤面外チェック
int checkOutPos(int x, int y) {
    return (x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE);
}

// 盤面の有効性チェック
int checkStoneStone(int board[BOARD_SIZE][BOARD_SIZE], int x, int y, int color) {
    if (checkOutPos(x, y)) return 0;
    return board[y][x] == color;
}

// Geminiへ手を送信して、Geminiの手(x, y)を返す
int ask_gemini_for_move(int board[BOARD_SIZE][BOARD_SIZE], int *x, int *y) {
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk = {0};

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (!curl) return -1;

    // --- 盤面をテキスト化 ---
    char board_text[4096] = "";
    for (int j = 0; j < BOARD_SIZE; j++) {
        for (int i = 0; i < BOARD_SIZE; i++) {
            char c = '.';
            if (board[j][i] == STONE_BLACK) c = 'X';
            else if (board[j][i] == STONE_WHITE) c = 'O';
            strncat(board_text, (char[]){c, '\0'}, 2);
        }
        strcat(board_text, "\\n");
    }

    char prompt_raw[8192];
    snprintf(prompt_raw, sizeof(prompt_raw),
        "You are an expert Gomoku player.\n"
        "Rules:\n"
        "- Board size: %dx%d.\n"
        "- X=Black, O=White, .=empty.\n"
        "- Goal: get exactly five in a row.\n"
        "- Return only JSON like {\"x\":7,\"y\":8} with your next move for WHITE (O).\n"
        "- Do not include any other text.\n"
        "Current board:\n%s",
        BOARD_SIZE, BOARD_SIZE, board_text);
    
    // --- JSON安全化 ---
    char prompt_escaped[16384];
    escape_json(prompt_raw, prompt_escaped, sizeof(prompt_escaped));
    
    // --- JSONリクエストを作成 ---
    char json_data[32768];
    snprintf(json_data, sizeof(json_data),
        "{\"contents\":[{\"parts\":[{\"text\":\"%s\"}]}]}",
        prompt_escaped);
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    char url[256];
    snprintf(url, sizeof(url),
        "https://generativelanguage.googleapis.com/v1/models/%s:generateContent?key=%s",
        GEMINI_MODEL, GEMINI_API_KEY);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    // --- リクエスト送信 ---
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl error: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        free(chunk.memory);
        return -1;
    }

    // --- デバッグ用: レスポンス全体を表示 ---
    printf("Gemini raw response:\n%s\n", chunk.memory);

    int gx = -1, gy = -1;

    if (chunk.memory) {
        // Gemini 2.5: candidates[0].content.parts[0].text を探す
        char *p = strstr(chunk.memory, "\"text\"");
        if (p) {
            p = strchr(p, ':');
            if (p) {
                p++;
                while (*p && (*p == ' ' || *p == '\"')) p++;
                // {"x":7,"y":8} 部分を抜き出す
                char json_buf[256] = {0};
                strncpy(json_buf, p, sizeof(json_buf) - 1);

                // {"x":7,"y":8} または類似パターンを解析
                if (sscanf(json_buf, "{\"x\":%d,\"y\":%d}", &gx, &gy) != 2) {
                    sscanf(json_buf, "%*[^0-9]%d%*[^0-9]%d", &gx, &gy);
                }
            }
        }
    }

    curl_easy_cleanup(curl);
    free(chunk.memory);
    curl_global_cleanup();

    // --- 応答チェック ---
    if (gx < 0 || gy < 0 || gx >= BOARD_SIZE || gy >= BOARD_SIZE) {
        fprintf(stderr, "[Gemini] Invalid move received (gx=%d, gy=%d)\n", gx, gy);
        return -1;
    }

    *x = gx;
    *y = gy;
    return 0;
}
//----------------------------------------
// メイン処理
//----------------------------------------
int main(void) {
    int board[BOARD_SIZE][BOARD_SIZE] = {{0}};
    int ai_color = STONE_BLACK;  // AI: 黒
    int gemini_color = STONE_WHITE;

    printf("Starting Gomoku AI vs Gemini...\n");

    while (1) {
        int x, y;

        // AIの手
        calcPutPos(board, ai_color, &x, &y);
        board[y][x] = ai_color;
        printf("AI move: %d %d\n", x, y);

        // 勝敗チェック
        if (checkWinner(board) != STONE_SPACE) {
            printf("AI wins!\n");
            break;
        }

        // Geminiの手
        int gx, gy;
        printf("Waiting for Gemini...\n");
        if (ask_gemini_for_move(board, &gx, &gy) == 0 && gx >= 0 && gy >= 0 && gx < BOARD_SIZE && gy < BOARD_SIZE) {
            board[gy][gx] = gemini_color;
            printf("[Gemini Model Used] %s\n", GEMINI_MODEL);
            printf("Gemini move: %d %d\n", gx, gy);
        } else {
            printf("Gemini failed to respond properly.\n");
            break;
        }

        // 勝敗チェック
        if (checkWinner(board) != STONE_SPACE) {
            printf("Gemini wins!\n");
            break;
        }
    }

    printf("Game over.\n");
    return 0;
}

int checkWinner(int board[BOARD_SIZE][BOARD_SIZE]) {
    // 横・縦・斜めの勝利判定（簡易版）
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            int stone = board[i][j];
            if (stone == STONE_SPACE) continue;
            // 横方向
            if (j <= BOARD_SIZE - 5 &&
                stone == board[i][j+1] &&
                stone == board[i][j+2] &&
                stone == board[i][j+3] &&
                stone == board[i][j+4])
                return stone;
            // 縦方向
            if (i <= BOARD_SIZE - 5 &&
                stone == board[i+1][j] &&
                stone == board[i+2][j] &&
                stone == board[i+3][j] &&
                stone == board[i+4][j])
                return stone;
            // 右下斜め
            if (i <= BOARD_SIZE - 5 && j <= BOARD_SIZE - 5 &&
                stone == board[i+1][j+1] &&
                stone == board[i+2][j+2] &&
                stone == board[i+3][j+3] &&
                stone == board[i+4][j+4])
                return stone;
            // 左下斜め
            if (i <= BOARD_SIZE - 5 && j >= 4 &&
                stone == board[i+1][j-1] &&
                stone == board[i+2][j-2] &&
                stone == board[i+3][j-3] &&
                stone == board[i+4][j-4])
                return stone;
        }
    }
    return STONE_SPACE; // 勝者なし
}
