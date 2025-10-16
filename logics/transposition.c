#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "transposition.h"

// Zobrist Hashing用の乱数テーブル
uint64_t zobrist_table[BOARD_SIZE][BOARD_SIZE][3];

// グローバル置換表
static TranspositionTable g_tt = {NULL, 0, 0, 0};

// ========================================
// Zobrist Hashing
// ========================================

// Zobristテーブルの初期化
void init_zobrist(void) {
    srand((unsigned int)time(NULL));

    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            for (int stone = 0; stone < 3; stone++) {
                // 64ビット乱数生成
                uint64_t r1 = (uint64_t)rand();
                uint64_t r2 = (uint64_t)rand();
                uint64_t r3 = (uint64_t)rand();
                zobrist_table[y][x][stone] = (r1 << 32) | (r2 << 16) | r3;
            }
        }
    }

    printf("[Zobrist] Initialized hash table\n");
}

// 盤面全体のハッシュ値を計算
uint64_t compute_zobrist_hash(const int board[][BOARD_SIZE]) {
    uint64_t hash = 0;

    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            int stone = board[y][x];
            if (stone != STONE_SPACE) {
                hash ^= zobrist_table[y][x][stone];
            }
        }
    }

    return hash;
}

// ハッシュ値の増分更新
uint64_t update_zobrist_hash(uint64_t hash, int x, int y, int stone) {
    return hash ^ zobrist_table[y][x][stone];
}

// ========================================
// 置換表
// ========================================

// 置換表の初期化
void init_transposition_table(int size) {
    if (g_tt.entries != NULL) {
        free(g_tt.entries);
    }

    g_tt.size = size;
    g_tt.entries = (TTEntry*)calloc(size, sizeof(TTEntry));
    g_tt.hits = 0;
    g_tt.queries = 0;

    if (g_tt.entries == NULL) {
        printf("[TT] Failed to allocate %d entries\n", size);
        g_tt.size = 0;
        return;
    }

    printf("[TT] Initialized with %d entries (~%.1f MB)\n",
           size, (size * sizeof(TTEntry)) / (1024.0 * 1024.0));
}

// 置換表の解放
void free_transposition_table(void) {
    if (g_tt.entries != NULL) {
        free(g_tt.entries);
        g_tt.entries = NULL;
        g_tt.size = 0;
    }
}

// 置換表のクリア
void clear_tt(void) {
    if (g_tt.entries != NULL) {
        memset(g_tt.entries, 0, g_tt.size * sizeof(TTEntry));
    }
    g_tt.hits = 0;
    g_tt.queries = 0;
}

// 置換表の検索
int probe_tt(uint64_t hash, int depth, int alpha, int beta, int *score, int *best_x, int *best_y) {
    if (g_tt.entries == NULL || g_tt.size == 0) {
        return 0;
    }

    g_tt.queries++;

    int index = (int)(hash % g_tt.size);
    TTEntry *entry = &g_tt.entries[index];

    // ハッシュが一致し、十分な深度で探索されている
    if (entry->hash == hash && entry->depth >= depth) {
        g_tt.hits++;

        *best_x = entry->best_x;
        *best_y = entry->best_y;

        // エントリタイプに応じて使用可否を判定
        if (entry->type == TT_EXACT) {
            *score = entry->score;
            return 1;
        } else if (entry->type == TT_ALPHA && entry->score <= alpha) {
            *score = alpha;
            return 1;
        } else if (entry->type == TT_BETA && entry->score >= beta) {
            *score = beta;
            return 1;
        }
    }

    return 0;
}

// 置換表への保存
void store_tt(uint64_t hash, int depth, int score, int type, int best_x, int best_y) {
    if (g_tt.entries == NULL || g_tt.size == 0) {
        return;
    }

    int index = (int)(hash % g_tt.size);
    TTEntry *entry = &g_tt.entries[index];

    // 既存エントリより深い探索結果、または新規エントリの場合に上書き
    if (entry->hash == 0 || entry->depth <= depth) {
        entry->hash = hash;
        entry->score = score;
        entry->depth = depth;
        entry->type = type;
        entry->best_x = best_x;
        entry->best_y = best_y;
    }
}

// 統計情報の表示
void print_tt_stats(void) {
    if (g_tt.queries > 0) {
        double hit_rate = (double)g_tt.hits / g_tt.queries * 100.0;
        // クロスプラットフォーム対応: Windows MinGWでは%I64u、その他では%llu
        #if defined(_WIN32) || defined(_WIN64)
            printf("[TT] Stats: %I64u queries, %I64u hits (%.1f%%)\n",
                   (uint64_t)g_tt.queries,
                   (uint64_t)g_tt.hits,
                   hit_rate);
        #else
            printf("[TT] Stats: %llu queries, %llu hits (%.1f%%)\n",
                   (unsigned long long)g_tt.queries,
                   (unsigned long long)g_tt.hits,
                   hit_rate);
        #endif
    }
}
