#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"

// グローバル設定（デフォルト値）
// 注意: config.txtが存在する場合、そちらの設定が優先されます
AIConfig g_config = {
    .search_depth = 4,                        // config.txtで上書き可能
    .tt_size = 262144,                        // config.txtで上書き可能
    .debug_mode = 0,                          // config.txtで上書き可能
    .timeout_ms = 10000,                      // config.txtで上書き可能
    .learning_mode = LEARNING_MODE_PRACTICE,  // config.txtで上書き可能
    .iterative_deepening = 1,                 // config.txtで上書き可能
    .debug_level = 1,                         // config.txtで上書き可能
    .id_time_threshold = 0.8,                 // config.txtで上書き可能
    .strict_time_limit_ms = 0,                // config.txtで上書き可能（0=timeout_msと同じ）
    .early_cutoff_time_threshold = 0.5        // config.txtで上書き可能
};

// デフォルト設定を使用
void use_default_config(void) {
    g_config.search_depth = 4;
    g_config.tt_size = 262144;
    g_config.debug_mode = 0;
    g_config.timeout_ms = 10000;
    g_config.learning_mode = LEARNING_MODE_PRACTICE;
    g_config.iterative_deepening = 1;
    g_config.debug_level = 1;
    g_config.id_time_threshold = 0.8;
    g_config.strict_time_limit_ms = 0;
    g_config.early_cutoff_time_threshold = 0.5;
}

// 設定ファイルから読み込み
// 重要: config.txtで設定された値は、プログラム内のデフォルト値やハードコーディングされた値よりも優先されます
int load_config(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("[Config] Cannot open %s, using defaults\n", filename);
        use_default_config();
        return 0;
    }

    char line[256];
    int loaded = 0;

    while (fgets(line, sizeof(line), fp)) {
        // コメント行と空行をスキップ
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        // キーと値を分離
        char key[128], value[128];
        if (sscanf(line, "%[^=]=%s", key, value) == 2) {
            if (strcmp(key, "SEARCH_DEPTH") == 0) {
                g_config.search_depth = atoi(value);
                printf("[Config] SEARCH_DEPTH set to %d\n", g_config.search_depth);
                if (g_config.search_depth < 1) g_config.search_depth = 1;
                if (g_config.search_depth > 10) g_config.search_depth = 10;
                loaded++;
            } else if (strcmp(key, "TRANSPOSITION_TABLE_SIZE") == 0) {
                g_config.tt_size = atoi(value);
                if (g_config.tt_size < 1024) g_config.tt_size = 1024;
                loaded++;
            } else if (strcmp(key, "DEBUG_MODE") == 0) {
                g_config.debug_mode = atoi(value);
                loaded++;
            } else if (strcmp(key, "TIMEOUT_MS") == 0) {
                g_config.timeout_ms = atoi(value);
                loaded++;
            } else if (strcmp(key, "LEARNING_MODE") == 0) {
                // 学習モードの設定を読み込み
                if (strcmp(value, "PRODUCTION") == 0) {
                    g_config.learning_mode = LEARNING_MODE_PRODUCTION;
                } else if (strcmp(value, "PRACTICE") == 0) {
                    g_config.learning_mode = LEARNING_MODE_PRACTICE;
                } else {
                    // デフォルトは練習モード
                    g_config.learning_mode = LEARNING_MODE_PRACTICE;
                }
                loaded++;
            } else if (strcmp(key, "ITERATIVE_DEEPENING") == 0) {
                g_config.iterative_deepening = atoi(value);
                loaded++;
            } else if (strcmp(key, "DEBUG_LEVEL") == 0) {
                g_config.debug_level = atoi(value);
                if (g_config.debug_level < 0) g_config.debug_level = 0;
                if (g_config.debug_level > 3) g_config.debug_level = 3;
                loaded++;
            } else if (strcmp(key, "ID_TIME_THRESHOLD") == 0) {
                g_config.id_time_threshold = atof(value);
                if (g_config.id_time_threshold < 0.0) g_config.id_time_threshold = 0.0;
                if (g_config.id_time_threshold > 1.0) g_config.id_time_threshold = 1.0;
                loaded++;
            } else if (strcmp(key, "STRICT_TIME_LIMIT_MS") == 0) {
                g_config.strict_time_limit_ms = atoi(value);
                if (g_config.strict_time_limit_ms < 0) g_config.strict_time_limit_ms = 0;
                loaded++;
            } else if (strcmp(key, "EARLY_CUTOFF_TIME_THRESHOLD") == 0) {
                g_config.early_cutoff_time_threshold = atof(value);
                if (g_config.early_cutoff_time_threshold < 0.0) g_config.early_cutoff_time_threshold = 0.0;
                if (g_config.early_cutoff_time_threshold > 1.0) g_config.early_cutoff_time_threshold = 1.0;
                loaded++;
            }
        }
    }

    fclose(fp);

    printf("[Config] ========================================\n");
    printf("[Config] Loaded from %s (PRIORITY)\n", filename);
    printf("[Config] These settings override all defaults\n");
    printf("[Config] ========================================\n");
    printf("  SEARCH_DEPTH = %d (from config.txt)\n", g_config.search_depth);
    printf("  TT_SIZE = %d (from config.txt)\n", g_config.tt_size);
    printf("  DEBUG_MODE = %d (from config.txt)\n", g_config.debug_mode);
    printf("  TIMEOUT_MS = %d (from config.txt)\n", g_config.timeout_ms);
    printf("  LEARNING_MODE = %s\n",
           g_config.learning_mode == LEARNING_MODE_PRODUCTION ? "PRODUCTION (本番)" : "PRACTICE (練習)");
    printf("  ITERATIVE_DEEPENING = %s\n", g_config.iterative_deepening ? "ON" : "OFF");
    printf("  DEBUG_LEVEL = %d ", g_config.debug_level);
    if (g_config.debug_level == 0) printf("(OFF)\n");
    else if (g_config.debug_level == 1) printf("(基本)\n");
    else if (g_config.debug_level == 2) printf("(詳細)\n");
    else printf("(超詳細)\n");
    printf("  ID_TIME_THRESHOLD = %.2f (%.0f%%)\n",
           g_config.id_time_threshold, g_config.id_time_threshold * 100);
    printf("  STRICT_TIME_LIMIT_MS = %d ms", g_config.strict_time_limit_ms);
    if (g_config.strict_time_limit_ms == 0) {
        printf(" (same as TIMEOUT_MS)\n");
    } else {
        printf("\n");
    }
    printf("  EARLY_CUTOFF_TIME_THRESHOLD = %.2f (%.0f%%)\n",
           g_config.early_cutoff_time_threshold, g_config.early_cutoff_time_threshold * 100);
    printf("[Config] ========================================\n");
    printf("[Config] All settings are active and will be used\n");
    printf("[Config] ========================================\n");

    return loaded;
}
