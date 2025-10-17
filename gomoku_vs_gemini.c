#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>   // sleep()
#include <curl/curl.h>
#include "gomoku_ai.h"
#include "gomoku.h"

int check = 0;

#define GEMINI_API_KEY "AIzaSyC5dd269eE8R-nY_4bmghL2_pX7itDIWIs"
#define GEMINI_MODEL "gemini-2.5-flash"

//============================================
// JSON関連・AI通信部分（変更なし）
//============================================
struct MemoryStruct {
    char *memory;
    size_t size;
};

// --- JSONエスケープ用関数 ---
void escape_json(const char *src, char *dst, size_t maxlen) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 2 < maxlen; i++) {
        if (src[i] == '\"') { dst[j++] = '\\'; dst[j++] = '\"'; }
        else if (src[i] == '\\') { dst[j++] = '\\'; dst[j++] = '\\'; }
        else if (src[i] == '\n') { dst[j++] = '\\'; dst[j++] = 'n'; }
        else { dst[j++] = src[i]; }
    }
    dst[j] = '\0';
}

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(ptr == NULL) return 0;
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

int checkOutPos(int x, int y) {
    return (x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE);
}

int checkStoneStone(int board[BOARD_SIZE][BOARD_SIZE], int x, int y, int color) {
    if (checkOutPos(x, y)) return 0;
    return board[y][x] == color;
}

//============================================
// Gemini応答関数（変更禁止部分）
//============================================
int ask_gemini_for_move(int board[BOARD_SIZE][BOARD_SIZE], int *x, int *y) {
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk = {0};

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (!curl) return -1;

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
    
    char prompt_escaped[16384];
    escape_json(prompt_raw, prompt_escaped, sizeof(prompt_escaped));

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
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

// --- 送信と再試行 ---
int retry_count = 0;
const int max_retries = 5;
static int ask_cnt = 0;

do {
    // POSTFIELDS 再設定（libcurlの再送対策）
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);

    res = curl_easy_perform(curl);

    if (res == CURLE_OK && chunk.memory && !strstr(chunk.memory, "\"503\"")) {
        break; // 成功
    }

    fprintf(stderr, "[Gemini] Server busy (503), retrying... (%d/%d)\n",
            retry_count + 1, max_retries);

    // 待機時間（段階的バックオフ）
    if (ask_cnt % 3 == 2) {
        fprintf(stderr, "[Gemini] Taking a longer cooldown (30 sec)\n");
        sleep(30);
    } else {
        sleep(10);
    }
    ask_cnt++;

    free(chunk.memory);
    chunk.memory = NULL;
    chunk.size = 0;

    retry_count++;
} while (retry_count < max_retries);


    printf("Gemini raw response:\n%s\n", chunk.memory);

    int gx = -1, gy = -1;
    if (chunk.memory) {
        char *p = strstr(chunk.memory, "\"text\"");
        if (p) {
            p = strchr(p, ':');
            if (p) {
                p++;
                while (*p && (*p == ' ' || *p == '\"')) p++;
                char json_buf[256] = {0};
                strncpy(json_buf, p, sizeof(json_buf) - 1);
                if (sscanf(json_buf, "{\"x\":%d,\"y\":%d}", &gx, &gy) != 2)
                    sscanf(json_buf, "%*[^0-9]%d%*[^0-9]%d", &gx, &gy);
            }
        }
    }

    curl_easy_cleanup(curl);
    free(chunk.memory);
    curl_global_cleanup();

    if (gx < 0 || gy < 0 || gx >= BOARD_SIZE || gy >= BOARD_SIZE) {
        fprintf(stderr, "[Gemini] Invalid move received (gx=%d, gy=%d)\n", gx, gy);
        return -1;
    }

    *x = gx;
    *y = gy;
    return 0;
}

//============================================
// 盤面・出力・勝敗管理
//============================================
void boardInit(int board[][BOARD_SIZE]) {
    for (int i = 0; i < BOARD_SIZE; i++)
        for (int j = 0; j < BOARD_SIZE; j++)
            board[i][j] = STONE_SPACE;
}

void boardPrint(int board[][BOARD_SIZE]) {
    printf("    ");
    for (int i = 0; i < BOARD_SIZE; i++) printf("%d ", i % 10);
    puts("");
    for (int i = 0; i < BOARD_SIZE; i++) {
        printf("%2d ", i);
        for (int j = 0; j < BOARD_SIZE; j++) {
            switch (board[i][j]) {
                case STONE_SPACE: printf("・"); break;
                case STONE_BLACK: printf("●"); break;
                case STONE_WHITE: printf("○"); break;
            }
        }
        puts("");
    }
    puts("");
}

void changeTurn(int *turn) {
    *turn = (*turn == STONE_BLACK) ? STONE_WHITE : STONE_BLACK;
}

int checkWinner(int board[BOARD_SIZE][BOARD_SIZE]) {
    for (int i = 0; i < BOARD_SIZE; i++)
        for (int j = 0; j < BOARD_SIZE; j++) {
            int s = board[i][j];
            if (s == STONE_SPACE) continue;
            if (j <= BOARD_SIZE-5 && s==board[i][j+1]&&s==board[i][j+2]&&s==board[i][j+3]&&s==board[i][j+4]) return s;
            if (i <= BOARD_SIZE-5 && s==board[i+1][j]&&s==board[i+2][j]&&s==board[i+3][j]&&s==board[i+4][j]) return s;
            if (i <= BOARD_SIZE-5 && j <= BOARD_SIZE-5 && s==board[i+1][j+1]&&s==board[i+2][j+2]&&s==board[i+3][j+3]&&s==board[i+4][j+4]) return s;
            if (i <= BOARD_SIZE-5 && j >= 4 && s==board[i+1][j-1]&&s==board[i+2][j-2]&&s==board[i+3][j-3]&&s==board[i+4][j-4]) return s;
        }
    return STONE_SPACE;
}

//============================================
// メイン処理
//============================================
int main(void) {
    int board[BOARD_SIZE][BOARD_SIZE];
    boardInit(board);
    int ai_color = STONE_BLACK;
    int gemini_color = STONE_WHITE;
    int turn = STONE_BLACK;

    printf("Starting Gomoku AI vs Gemini...\n");
    boardPrint(board);

    while (1) {
        int x, y;

        if (turn == ai_color) {
            calcPutPos(board, ai_color, &x, &y);
            board[y][x] = ai_color;
            printf("AI move: %d %d\n", x, y);
        } else {
            printf("Waiting for Gemini...\n");
            if (ask_gemini_for_move(board, &x, &y) == 0) {
                board[y][x] = gemini_color;
                printf("Gemini move: %d %d\n", x, y);
            } else {
                printf("Gemini failed to respond properly.\n");
                break;
            }
        }

        boardPrint(board);
        int winner = checkWinner(board);
        if (winner != STONE_SPACE) {
            if (winner == ai_color) printf("AI wins!\n");
            else printf("Gemini wins!\n");
            break;
        }

        changeTurn(&turn);
    }

    printf("Game over.\n");
    return 0;
}
