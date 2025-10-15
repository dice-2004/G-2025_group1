#ifndef CONFIG_H
#define CONFIG_H

#include "logics/learning.h"

// AI設定構造体
typedef struct {
    int search_depth;
    int tt_size;
    int debug_mode;
    int timeout_ms;
    LearningMode learning_mode;  // 学習モード（PRACTICE/PRODUCTION）
    int iterative_deepening;     // 反復深化探索（0=OFF, 1=ON）
    int debug_level;             // デバッグレベル（0-3）
    double id_time_threshold;    // ID時間制限閾値（0.0～1.0）
    int strict_time_limit_ms;    // 厳格な時間制限（ミリ秒、0=timeout_msと同じ）
} AIConfig;

// グローバル設定
extern AIConfig g_config;

// 設定の読み込み
int load_config(const char *filename);

// デフォルト設定の使用
void use_default_config(void);

#endif
