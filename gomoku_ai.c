//------------------------------------------------------------------------
// 五目ならべAI
//  ここを作る．
//  下記のサンプルは，直前に自分が置いた石の8近傍のどこかにランダムに石を置くだけ
//
//  引数説明
//    board[BOARD_SIZE][BORARD_SIZE]：五目並べの盤
//    com ： コンピュータが白石(STONE_WHITE)か黒石(STONE_BLACK)かを表す．
//    pos_x, pos_y：石を置く場所のx座標，y座標 両方出力用
//------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include "gomoku.h"
#include "gomoku_ai.h"
#include "config.h"
#include "logics/main_engine.h"
#include "logics/TSS.h"
#include "logics/evaluation.h"
#include "logics/transposition.h"
#include "logics/opening_book.h"
#include "logics/learning.h"
#include "logics/debug_utils.h"

// .libgomoku_to.c の禁じ手判定関数を使用（AI思考用：ログなし）
extern int isSan_to(int board[][BOARD_SIZE], int x, int y);
extern int isShi_to(int board[][BOARD_SIZE], int x, int y);
extern int isRoku_to(int board[][BOARD_SIZE], int x, int y);

// ビットマップ関連の定義は gomoku_ai.h で定義済み

// 斜めビットテーブルとインデックスの変換表
static int pos_dr[N][N];
static int pos_dl[N][N];

// 斜めビットテーブル初期化フラグ　実初期化:0, 初期化済:1
static int pos_dr_table_is_init = 0;
static int pos_dl_table_is_init = 0;


// 盤面の石数をカウント
static int count_stones(const int board[][BOARD_SIZE]) {
    int count = 0;
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (board[y][x] != STONE_SPACE) count++;
        }
    }
    return count;
}

// 前回の盤面を保存（差分検出用）
static int g_previous_board[BOARD_SIZE][BOARD_SIZE];
static int g_previous_move_count = 0;
static int g_ai_color = STONE_SPACE;

// AIの色を取得（外部から呼び出し可能）
int get_ai_color(void) {
    return g_ai_color;
}

// 盤面から勝者を判定
static int detect_winner(const int board[][BOARD_SIZE]) {
    // 5連を探す
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (board[y][x] == STONE_SPACE) continue;
            int color = board[y][x];

            // 4方向をチェック
            int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
            for (int d = 0; d < 4; d++) {
                int dx = directions[d][0];
                int dy = directions[d][1];
                int count = 1;

                // 正方向
                for (int i = 1; i < 5; i++) {
                    int nx = x + dx * i;
                    int ny = y + dy * i;
                    if (nx < 0 || nx >= BOARD_SIZE || ny < 0 || ny >= BOARD_SIZE) break;
                    if (board[ny][nx] == color) count++;
                    else break;
                }

                // 負方向
                for (int i = 1; i < 5; i++) {
                    int nx = x - dx * i;
                    int ny = y - dy * i;
                    if (nx < 0 || nx >= BOARD_SIZE || ny < 0 || ny >= BOARD_SIZE) break;
                    if (board[ny][nx] == color) count++;
                    else break;
                }

                if (count >= 5) return color;
            }
        }
    }
    return STONE_SPACE;
}

// 差分検出：前回から追加された手を記録
static void detect_and_record_new_moves(const int board[][BOARD_SIZE]) {
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (board[y][x] != STONE_SPACE && g_previous_board[y][x] == STONE_SPACE) {
                // 新しく置かれた石を発見
                record_move(x, y, board[y][x]);
            }
        }
    }
}

int calcPutPos(int board[][BOARD_SIZE], int com, int *pos_x, int *pos_y){
    static int initialized = 0;

    // 初回のみ初期化処理
    if (!initialized) {
        srand((unsigned int)time(NULL));

        // 設定ファイル読み込み
        load_config("config.txt");

        // パターンスコア読み込み（拡張版を使用 - 20パターン完全対応）
        load_pattern_scores("patterns_extended.csv");

        // 序盤定石読み込み
        load_opening_book("opening_book.csv");

        // Zobrist Hashing初期化
        init_zobrist();

        // 置換表初期化
        init_transposition_table(g_config.tt_size);

        // 学習システム初期化
        init_learning();
        load_learning_data();

        // 設定ファイルから学習モードを読み込んで適用
        set_learning_mode(g_config.learning_mode);

        // 前回盤面を初期化
        for (int y = 0; y < BOARD_SIZE; y++) {
            for (int x = 0; x < BOARD_SIZE; x++) {
                g_previous_board[y][x] = STONE_SPACE;
            }
        }

        initialized = 1;
        printf("[AI] Initialization complete.\n");
        printf("=================================================\n");
    }

    // 石数をカウント
    int move_count = count_stones(board);

    // AIの色を記録
    if (g_ai_color == STONE_SPACE) {
        g_ai_color = com;
        set_ai_color(com);  // 学習システムにも通知
    }

    // ========================================
    // 前回対戦の勝敗判定と学習
    // ========================================
    if (move_count == 0 && g_previous_move_count > 0) {
        // 新しい対戦が始まった（盤面がリセットされた）
        // 前回の勝者を判定して学習
        printf("[AI] New game detected (move_count=0, previous=%d)\n", g_previous_move_count);
        int winner = detect_winner(g_previous_board);
        printf("[AI] Previous game winner: %d (AI color: %d)\n", winner, g_ai_color);

        if (winner != STONE_SPACE) {
            int result = (winner == g_ai_color) ? 1 : -1;
            printf("[AI] Calling finish_game_record with result=%d\n", result);
            finish_game_record(result);
        } else {
            printf("[AI] No winner detected in previous game\n");
        }

        // 新規対戦開始
        printf("[AI] Starting new game record\n");
        start_game_record();
    } else if (move_count == 0) {
        // 初回対戦開始
        printf("[AI] First game ever - starting game record\n");
        start_game_record();
    }

    // ========================================
    // 前回から追加された手を記録
    // ========================================
    if (move_count > g_previous_move_count) {
        detect_and_record_new_moves(board);

        // プレイヤーが既に勝っているかチェック
        int winner = detect_winner(board);
        if (winner != STONE_SPACE) {
            printf("[AI] Game already finished! Winner: %d (AI color: %d)\n", winner, g_ai_color);
            int result = (winner == g_ai_color) ? 1 : -1;
            printf("[AI] Recording game result: %d\n", result);
            finish_game_record(result);
            start_game_record();  // 新規ゲーム開始
        }
    }

    // 現在の盤面を保存
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            g_previous_board[y][x] = board[y][x];
        }
    }
    g_previous_move_count = move_count;

    // ビットマップのデバッグ表示（オプション）
#ifdef DEBUG_BITMAPS
    {
        AllMaps maps_local;
        build_maps(board, &maps_local);
        static char outbuf[TOTAL + 1];
        outbuf[TOTAL] = '\0';

        printf("=== Bitmap Debug Info ===\n");
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = getbit(maps_local.row.black, i) ? '1' : '0';
        printf("[Bitmap row] black=%s\n", outbuf);
        for (int i = 0; i < TOTAL; ++i) outbuf[i] = getbit(maps_local.row.white, i) ? '1' : '0';
        printf("[Bitmap row] white=%s\n", outbuf);
    }
#endif

    printf("\n[AI] Move %d thinking...\n", move_count + 1);

    // デバッグ: 盤面表示
    if (g_config.debug_level >= 2) {
        debug_print_board(board, "Current Board State");
    }

    // ========================================
    // フェーズ1: 詰みと必死のチェック (TSS) - 最優先
    // ========================================
    int win_x, win_y;
    int attack_level;

    // 自分の勝ち手をチェック（五連）
    if (find_winning_move(board, com, &win_x, &win_y)) {
        // 禁じ手チェック（黒の場合）
        if (com == STONE_BLACK && is_forbidden(board, win_x, win_y, com)) {
            if (g_config.debug_level >= 1) {
                printf("[AI] Winning move at (%d,%d) is forbidden! Skipping...\n", win_x, win_y);
            }
        } else {
            *pos_x = win_x;
            *pos_y = win_y;
            printf("[AI] Winning move executed!\n");
            return 0;
        }
    }

    // 相手の勝ち手を防ぐ（五連防御）
    if (find_threat_move(board, com, &win_x, &win_y)) {
        // 防御手は禁じ手チェック不要（相手の手を防ぐだけ）
        // ただし、念のためチェック
        if (com == STONE_BLACK && is_forbidden(board, win_x, win_y, com)) {
            if (g_config.debug_level >= 1) {
                printf("[AI] Threat defense at (%d,%d) is forbidden! Skipping...\n", win_x, win_y);
            }
        } else {
            *pos_x = win_x;
            *pos_y = win_y;
            printf("[AI] Threat defended!\n");
            return 0;
        }
    }

    // 自分の開いた四をチェック（最優先攻撃）
    attack_level = find_strong_attack_move(board, com, &win_x, &win_y);
    if (attack_level == 2) {  // 開いた四
        // 禁じ手チェック（黒の場合）
        if (com == STONE_BLACK && is_forbidden(board, win_x, win_y, com)) {
            if (g_config.debug_level >= 1) {
                printf("[AI] Open four at (%d,%d) is forbidden! Skipping...\n", win_x, win_y);
            }
            attack_level = 0;  // レベルをリセット
        } else {
            *pos_x = win_x;
            *pos_y = win_y;
            printf("[AI] Open four attack executed!\n");
            return 0;
        }
    }

    // 相手の開いた四を防ぐ（最優先防御）
    int defense_level = find_strong_defense_move(board, com, &win_x, &win_y);
    if (defense_level == 2) {
        // 禁じ手チェック（黒の場合）
        if (com == STONE_BLACK && is_forbidden(board, win_x, win_y, com)) {
            if (g_config.debug_level >= 1) {
                printf("[AI] Defense of open four at (%d,%d) is forbidden! Skipping...\n", win_x, win_y);
            }
            defense_level = 0;  // レベルをリセット
        } else {
            *pos_x = win_x;
            *pos_y = win_y;
            printf("[AI] Defended opponent's open four!\n");
            return 0;
        }
    }

    // 開いた三・間空き三は削除（過剰防御のため）
    // → Alpha-Beta探索で評価

    // ========================================
    // フェーズ2: 序盤定石のチェック（脅威手より後に実行）
    // ========================================
    // 定石は10手目まで（両者合わせて）
    // move_count = 0-9 → move_number = 1-10

    if (move_count < 10) {
        int book_x, book_y;
        if (find_opening_move(board, move_count, com, &book_x, &book_y)) {
            *pos_x = book_x;
            *pos_y = book_y;
            printf("[AI] Opening book move: (%d, %d)\n", *pos_x, *pos_y);
            return 0;
        }
    }

    // 初手の場合は中央（定石が見つからなかった場合）
    if (move_count == 0) {
        *pos_y = BOARD_SIZE / 2;
        *pos_x = BOARD_SIZE / 2;
        printf("[AI] First move at center (%d, %d)\n", *pos_x, *pos_y);
        return 0;
    }

    // 開いた三の防御は削除（過剰防御のため）
    // → Alpha-Beta探索で評価

    // ========================================
    // フェーズ4: Alpha-Beta探索 or 反復深化探索
    // ========================================
    int best_x, best_y;
    int score;

    // 反復深化探索を使用するかどうか
    if (g_config.iterative_deepening) {
        // 反復深化探索（深さ1から徐々に探索）
        printf("[AI] Starting Iterative Deepening search...\n");
        score = iterative_deepening(board, com, g_config.timeout_ms, &best_x, &best_y);
    } else {
        // 通常のAlpha-Beta探索（固定深度）
        int depth = g_config.search_depth;
        printf("[AI] Starting Alpha-Beta search (depth=%d)...\n", depth);
        score = alpha_beta_search(board, com, depth, &best_x, &best_y);
    }

    *pos_x = best_x;
    *pos_y = best_y;

    // 手を記録（学習用）
    record_move(*pos_x, *pos_y, com);

    printf("[AI] Final move: (%d, %d) with score %d\n", *pos_x, *pos_y, score);

    // AIの手を盤面に反映して勝敗判定（学習用）
    int temp_board[BOARD_SIZE][BOARD_SIZE];
    memcpy(temp_board, board, sizeof(temp_board));
    temp_board[*pos_y][*pos_x] = com;

    // 勝敗判定
    int winner = detect_winner(temp_board);
    if (winner == com) {
        printf("[AI] AI wins! Recording victory.\n");
        finish_game_record(1);  // AI勝利
        // 新規ゲームを開始
        start_game_record();
    } else if (winner != STONE_SPACE) {
        printf("[AI] AI loses! Recording defeat.\n");
        finish_game_record(-1);  // AI敗北
        // 新規ゲームを開始
        start_game_record();
    }

    // デバッグ: 評価値の詳細
    if (g_config.debug_level >= 2) {
        debug_print_evaluation(board, com);
    }

    printf("=================================================\n");

    return 0;
}

// ========================================
// ビットボード基本操作関数
// ========================================

// ビットマップにビットセット
void setbit(uint64_t arr[], int pos) {
    size_t wi = pos / 64;
    size_t bi = pos % 64;
    arr[wi] |= (uint64_t)1ULL << (63 - bi);
}

// ビットマップからビット取得
int getbit(const uint64_t arr[], int pos) {
    return (int)((arr[pos / 64] >> (63 - (pos % 64))) & 1ULL);
}

// ビットマップからビットクリア
void clearbit(uint64_t arr[], int pos) {
    size_t wi = pos / 64;
    size_t bi = pos % 64;
    arr[wi] &= ~((uint64_t)1ULL << (63 - bi));
}

// 縦横のインデックス変換ヘルパー関数
int pos_row(int x, int y) { return y * N + x; }
int pos_col(int x, int y) { return x * N + y; }

// 斜め(右下)のインデックス変換関数
int pos_diag_dr(int x, int y) {
    if (!pos_dr_table_is_init) init_pos_tables();
    return pos_dr[x][y];
}

// 斜め(左下)のインデックス変換関数
int pos_diag_dl(int x, int y) {
    if (!pos_dl_table_is_init) init_pos_tables();
    return pos_dl[x][y];
}

// ========================================
// 座標変換テーブル初期化
// ========================================

// 斜めビットテーブルto インデックス変換表初期化ルーチン
void init_pos_tables(void) {
    if (pos_dr_table_is_init && pos_dl_table_is_init) return;
    int p = 0;
    for (int k = -(N - 1); k <= (N - 1); ++k) {
        for (int y = 0; y < N; ++y) {
            int x = y + k;
            if (x >= 0 && x < N) {
                pos_dr[x][y] = p;
                ++p;
            }
        }
    }
    pos_dr_table_is_init = 1;

    p = 0;
    for (int k = 0; k <= 2 * (N - 1); ++k) {
        // 参考三項演算子
        int y0 = (k - (N - 1) > 0) ? (k - (N - 1)) : 0;
        int y1 = (k < (N - 1)) ? k : (N - 1);
        for (int y = y0; y <= y1; ++y) {
            int x = k - y;
            if (x >= 0 && x < N) {
                pos_dl[x][y] = p;
                ++p;
            }
        }
    }
    pos_dl_table_is_init = 1;
}

// ========================================
// 盤面変換関数
// ========================================

// ビットマップ生成ルーチン（int配列 → ビットボード）
void build_maps(const int board[][BOARD_SIZE], AllMaps *maps) {

    memset(maps, 0, sizeof(AllMaps));
    init_pos_tables();

    // 横
    int p;
    p = 0;
    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            int v = board[y][x];
            if (v == STONE_BLACK) setbit(maps->row.black, p);
            if (v == STONE_WHITE) setbit(maps->row.white, p);
            if (v != STONE_SPACE) setbit(maps->row.fill, p);
            ++p;
        }
    }

    // 縦
    p = 0;
    for (int x = 0; x < N; ++x) {
        for (int y = 0; y < N; ++y) {
            int v = board[y][x];
            if (v == STONE_BLACK) setbit(maps->col.black, p);
            if (v == STONE_WHITE) setbit(maps->col.white, p);
            if (v != STONE_SPACE) setbit(maps->col.fill, p);
            ++p;
        }
    }

    // 右斜め
    p = 0;
    for (int k = -(N - 1); k <= (N - 1); ++k) {
        for (int y = 0; y < N; ++y) {
            int x = y + k;
            if (x >= 0 && x < N) {
                int v = board[y][x];
                if (v == STONE_BLACK) setbit(maps->diag_dr.black, p);
                if (v == STONE_WHITE) setbit(maps->diag_dr.white, p);
                if (v != STONE_SPACE) setbit(maps->diag_dr.fill, p);
                ++p;
            }
        }
    }

    // 左斜め
    p = 0;
    for (int k = 0; k <= 2 * (N - 1); ++k) {
        int y0 = (k - (N - 1) > 0) ? (k - (N - 1)) : 0;
        int y1 = (k < (N - 1)) ? k : (N - 1);
        for (int y = y0; y <= y1; ++y) {
            int x = k - y;
            if (x >= 0 && x < N) {
                int v = board[y][x];
                if (v == STONE_BLACK) setbit(maps->diag_dl.black, p);
                if (v == STONE_WHITE) setbit(maps->diag_dl.white, p);
                if (v != STONE_SPACE) setbit(maps->diag_dl.fill, p);
                ++p;
            }
        }
    }
}

// ビットボード → int配列への逆変換
void maps_to_board(const AllMaps *maps, int board[][BOARD_SIZE]) {
    // row-majorビットマップから盤面を復元
    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            int p = pos_row(x, y);
            if (getbit(maps->row.black, p)) {
                board[y][x] = STONE_BLACK;
            } else if (getbit(maps->row.white, p)) {
                board[y][x] = STONE_WHITE;
            } else {
                board[y][x] = STONE_SPACE;
            }
        }
    }
}

// ビットボード上での石の配置
void place_stone(AllMaps *maps, int x, int y, int color) {
    init_pos_tables();

    // 各方向にビットを立てる
    int p_row = pos_row(x, y);
    int p_col = pos_col(x, y);
    int p_dr = pos_diag_dr(x, y);
    int p_dl = pos_diag_dl(x, y);

    if (color == STONE_BLACK) {
        setbit(maps->row.black, p_row);
        setbit(maps->col.black, p_col);
        setbit(maps->diag_dr.black, p_dr);
        setbit(maps->diag_dl.black, p_dl);
    } else if (color == STONE_WHITE) {
        setbit(maps->row.white, p_row);
        setbit(maps->col.white, p_col);
        setbit(maps->diag_dr.white, p_dr);
        setbit(maps->diag_dl.white, p_dl);
    }

    setbit(maps->row.fill, p_row);
    setbit(maps->col.fill, p_col);
    setbit(maps->diag_dr.fill, p_dr);
    setbit(maps->diag_dl.fill, p_dl);
}

// ビットボード上での石の削除
void remove_stone(AllMaps *maps, int x, int y, int color) {
    init_pos_tables();

    // 各方向のビットをクリア
    int p_row = pos_row(x, y);
    int p_col = pos_col(x, y);
    int p_dr = pos_diag_dr(x, y);
    int p_dl = pos_diag_dl(x, y);

    if (color == STONE_BLACK) {
        clearbit(maps->row.black, p_row);
        clearbit(maps->col.black, p_col);
        clearbit(maps->diag_dr.black, p_dr);
        clearbit(maps->diag_dl.black, p_dl);
    } else if (color == STONE_WHITE) {
        clearbit(maps->row.white, p_row);
        clearbit(maps->col.white, p_col);
        clearbit(maps->diag_dr.white, p_dr);
        clearbit(maps->diag_dl.white, p_dl);
    }

    clearbit(maps->row.fill, p_row);
    clearbit(maps->col.fill, p_col);
    clearbit(maps->diag_dr.fill, p_dr);
    clearbit(maps->diag_dl.fill, p_dl);
}

// ========================================
// 禁じ手判定ヘルパー関数
// ========================================

// 特定方向の連続石数をカウント
static int count_line(const int board[][BOARD_SIZE], int x, int y, int dx, int dy, int color) {
    int count = 0;
    int nx = x + dx;
    int ny = y + dy;

    while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE) {
        if (board[ny][nx] == color) {
            count++;
            nx += dx;
            ny += dy;
        } else {
            break;
        }
    }
    return count;
}

// 特定方向で「三」を形成するかチェック
static int check_three_in_direction(const int board[][BOARD_SIZE], int x, int y, int dx, int dy) {
    int temp_board[BOARD_SIZE][BOARD_SIZE];
    memcpy(temp_board, board, sizeof(temp_board));
    temp_board[y][x] = STONE_BLACK;

    int forward = count_line(temp_board, x, y, dx, dy, STONE_BLACK);
    int backward = count_line(temp_board, x, y, -dx, -dy, STONE_BLACK);
    int total = forward + backward + 1;

    // 三になるか判定（両端が開いている3連）
    if (total == 3) {
        int fx = x + (forward + 1) * dx;
        int fy = y + (forward + 1) * dy;
        int bx = x - (backward + 1) * dx;
        int by = y - (backward + 1) * dy;

        bool f_open = (fx >= 0 && fx < BOARD_SIZE && fy >= 0 && fy < BOARD_SIZE &&
                       temp_board[fy][fx] == STONE_SPACE);
        bool b_open = (bx >= 0 && bx < BOARD_SIZE && by >= 0 && by < BOARD_SIZE &&
                       temp_board[by][bx] == STONE_SPACE);

        if (f_open && b_open) return 1;
    }
    return 0;
}

// 特定方向で「四」を形成するかチェック
static int check_four_in_direction(const int board[][BOARD_SIZE], int x, int y, int dx, int dy) {
    int temp_board[BOARD_SIZE][BOARD_SIZE];
    memcpy(temp_board, board, sizeof(temp_board));
    temp_board[y][x] = STONE_BLACK;

    int forward = count_line(temp_board, x, y, dx, dy, STONE_BLACK);
    int backward = count_line(temp_board, x, y, -dx, -dy, STONE_BLACK);
    int total = forward + backward + 1;

    // 四になるか判定
    if (total == 4) {
        int fx = x + (forward + 1) * dx;
        int fy = y + (forward + 1) * dy;
        int bx = x - (backward + 1) * dx;
        int by = y - (backward + 1) * dy;

        bool f_open = (fx >= 0 && fx < BOARD_SIZE && fy >= 0 && fy < BOARD_SIZE &&
                       temp_board[fy][fx] == STONE_SPACE);
        bool b_open = (bx >= 0 && bx < BOARD_SIZE && by >= 0 && by < BOARD_SIZE &&
                       temp_board[by][bx] == STONE_SPACE);

        if (f_open || b_open) return 1;
    }
    return 0;
}

// ========================================
// 禁じ手判定
// ========================================

// 三三のチェック
int check_sansan(const int board[][BOARD_SIZE], int x, int y) {
    int three_count = 0;

    // 4方向をチェック
    int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
    for (int i = 0; i < 4; i++) {
        if (check_three_in_direction(board, x, y, directions[i][0], directions[i][1])) {
            three_count++;
        }
    }

    return (three_count >= 2) ? 1 : 0;
}

// 四四のチェック
int check_shishi(const int board[][BOARD_SIZE], int x, int y) {
    int four_count = 0;

    // 4方向をチェック
    int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
    for (int i = 0; i < 4; i++) {
        if (check_four_in_direction(board, x, y, directions[i][0], directions[i][1])) {
            four_count++;
        }
    }

    return (four_count >= 2) ? 1 : 0;
}

// 長連のチェック
int check_choren(const int board[][BOARD_SIZE], int x, int y) {
    int temp_board[BOARD_SIZE][BOARD_SIZE];
    memcpy(temp_board, board, sizeof(temp_board));
    temp_board[y][x] = STONE_BLACK;

    // 4方向をチェック
    int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
    for (int i = 0; i < 4; i++) {
        int dx = directions[i][0];
        int dy = directions[i][1];
        int forward = count_line(temp_board, x, y, dx, dy, STONE_BLACK);
        int backward = count_line(temp_board, x, y, -dx, -dy, STONE_BLACK);
        int total = forward + backward + 1;

        if (total >= 6) return 1;  // 6連以上は長連
    }

    return 0;
}

// 総合的な禁じ手判定 (.libgomoku_to.cの判定ロジックを使用：AI思考用ログなし版)
int is_forbidden(const int board[][BOARD_SIZE], int x, int y, int color) {
    // 白石の場合は禁じ手なし
    if (color != STONE_BLACK) return 0;

    // 既に石がある場合は置けない
    if (board[y][x] != STONE_SPACE) return 1;

    // 一時的に盤面コピーして石を置いてみる
    int temp_board[BOARD_SIZE][BOARD_SIZE];
    memcpy(temp_board, board, sizeof(temp_board));
    temp_board[y][x] = STONE_BLACK;

    // .libgomoku_to.c の禁じ手判定関数を使用（AI思考用：ログなし）
    // check変数で有効な禁じ手をチェック（gomoku.hで定義）

    // 長連チェック (check & CHOLEN)
    if ((check & CHOLEN) && isRoku_to(temp_board, x, y)) {
        return 1;
    }

    // 三三チェック (check & SANSAN)
    if ((check & SANSAN)) {
        int san_value = isSan_to(temp_board, x, y);
        // isSan_to()は三が見つかった方向の合計値を返す
        // LEFT=1, TATE=2, YOKO=4, RIGT=8
        // 複数方向の場合（単一方向でない場合）は三三
        if (san_value > 0 &&
            san_value != LEFT && san_value != TATE &&
            san_value != YOKO && san_value != RIGT) {
            return 1;
        }
    }

    // 四四チェック (check & SHISHI)
    if ((check & SHISHI)) {
        int shi_value = isShi_to(temp_board, x, y);
        // 複数方向の場合（単一方向でない場合）は四四
        if (shi_value > 0 &&
            shi_value != LEFT && shi_value != TATE &&
            shi_value != YOKO && shi_value != RIGT) {
            return 1;
        }
    }

    return 0;
}

// ========================================
// 合法手生成
// ========================================

// 合法手を生成（既に石が置かれている周辺のみを候補とする）
int generate_legal_moves(const int board[][BOARD_SIZE], int color, Move moves[], int max_moves) {
    int move_count = 0;
    bool has_stone = false;

    // 盤面に石があるかチェック
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (board[y][x] != STONE_SPACE) {
                has_stone = true;
                break;
            }
        }
        if (has_stone) break;
    }

    // 初手の場合は中央のみ
    if (!has_stone) {
        if (move_count < max_moves) {
            moves[move_count].x = BOARD_SIZE / 2;
            moves[move_count].y = BOARD_SIZE / 2;
            move_count++;
        }
        return move_count;
    }

    // 既存の石の周辺2マスを候補とする
    bool candidate[BOARD_SIZE][BOARD_SIZE] = {false};

    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (board[y][x] != STONE_SPACE) {
                // 周辺2マスをマーク
                for (int dy = -2; dy <= 2; dy++) {
                    for (int dx = -2; dx <= 2; dx++) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE) {
                            if (board[ny][nx] == STONE_SPACE) {
                                candidate[ny][nx] = true;
                            }
                        }
                    }
                }
            }
        }
    }

    // 候補から合法手を抽出
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (candidate[y][x] && move_count < max_moves) {
                // 禁じ手チェック
                if (!is_forbidden(board, x, y, color)) {
                    moves[move_count].x = x;
                    moves[move_count].y = y;
                    move_count++;
                }
            }
        }
    }

    return move_count;
}
