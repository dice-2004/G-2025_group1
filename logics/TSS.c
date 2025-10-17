#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <limits.h>
#include "TSS.h"
#include "evaluation.h"
#include "../gomoku_ai.h"
#include "../config.h"

// ========================================
// TSS: Threat Space Search（詰み探索）
// ========================================

// 特定位置に石を置いて五連になるかチェック
static int makes_five(const int board[][BOARD_SIZE], int x, int y, int color) {
    int temp_board[BOARD_SIZE][BOARD_SIZE];
    memcpy(temp_board, board, sizeof(temp_board));
    temp_board[y][x] = color;

    // 4方向をチェック
    int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

    for (int d = 0; d < 4; d++) {
        int dx = directions[d][0];
        int dy = directions[d][1];
        int count = 1;

        // 正方向をカウント
        int nx = x + dx;
        int ny = y + dy;
        while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE &&
               temp_board[ny][nx] == color) {
            count++;
            nx += dx;
            ny += dy;
        }

        // 逆方向をカウント
        nx = x - dx;
        ny = y - dy;
        while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE &&
               temp_board[ny][nx] == color) {
            count++;
            nx -= dx;
            ny -= dy;
        }

        if (count == 5) return 1;  // ちょうど5連
    }

    return 0;
}

// ========================================
// 勝ち手探索
// ========================================

// 自分が次に打って勝てる手を探す
int find_winning_move(const int board[][BOARD_SIZE], int color, int *win_x, int *win_y) {
    // 全ての空きマスをチェック
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (board[y][x] == STONE_SPACE) {
                // 禁じ手チェック（黒の場合）
                if (color == STONE_BLACK && is_forbidden(board, x, y, color)) {
                    continue;
                }

                // この手で五連ができるか
                if (makes_five(board, x, y, color)) {
                    *win_x = x;
                    *win_y = y;
                    if (g_config.debug_level >= 1) {
                        printf("[TSS-WinningMove] Five-in-a-row found at (%d, %d)\n", x, y);
                    }
                    return 1;
                }
            }
        }
    }

    return 0;
}

// ========================================
// 必死（相手の勝ち手を防ぐ）
// ========================================

// 相手が次に打って勝つ手を探す
int find_threat_move(const int board[][BOARD_SIZE], int color, int *threat_x, int *threat_y) {
    int opponent = (color == STONE_BLACK) ? STONE_WHITE : STONE_BLACK;

    // 相手の勝ち手を探す
    if (find_winning_move(board, opponent, threat_x, threat_y)) {
        if (g_config.debug_level >= 1) {
            printf("[TSS-ThreatMove] Opponent's winning threat at (%d, %d)\n", *threat_x, *threat_y);
        }
        return 1;
    }

    return 0;
}

// ========================================
// 高度な詰み探索（開いた四・開いた三）
// ========================================

// 特定方向に石を数える
static __attribute__((unused)) int count_stones_in_direction(const int board[][BOARD_SIZE], int x, int y, int dx, int dy, int color, int *spaces_before_block) {
    int count = 0;
    int spaces = 0;
    int nx = x + dx;
    int ny = y + dy;

    while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE) {
        if (board[ny][nx] == color) {
            count++;
        } else if (board[ny][nx] == STONE_SPACE) {
            spaces++;
            if (spaces > 1) break;  // 空きが2マス以上続いたら終了
        } else {
            break;  // 相手の石または端
        }
        nx += dx;
        ny += dy;
    }

    *spaces_before_block = spaces;
    return count;
}

// 開いた四（両端が空いている4連）を検出
// board[y][x]は空きマス。ここに石を置いたら4連両空きができるかチェック
static int has_open_four(const int board[][BOARD_SIZE], int x, int y, int color) {
    if (board[y][x] != STONE_SPACE) {
        return 0;
    }

    int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

    for (int d = 0; d < 4; d++) {
        int dx = directions[d][0];
        int dy = directions[d][1];

        // 左右の連続する同色の石をカウント
        int left_count = 0;
        for (int i = 1; i <= 3; i++) {
            int nx = x - dx * i;
            int ny = y - dy * i;
            if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board[ny][nx] == color) {
                left_count++;
            } else {
                break;
            }
        }

        int right_count = 0;
        for (int i = 1; i <= 3; i++) {
            int nx = x + dx * i;
            int ny = y + dy * i;
            if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board[ny][nx] == color) {
                right_count++;
            } else {
                break;
            }
        }

        // この位置を含めて4連になるか？（左+1+右 == 4）
        if (left_count + right_count == 3) {
            // 両端が空いているかチェック
            int left_end_x = x - dx * (left_count + 1);
            int left_end_y = y - dy * (left_count + 1);
            int right_end_x = x + dx * (right_count + 1);
            int right_end_y = y + dy * (right_count + 1);

            int left_open = (left_end_x >= 0 && left_end_x < BOARD_SIZE &&
                           left_end_y >= 0 && left_end_y < BOARD_SIZE &&
                           board[left_end_y][left_end_x] == STONE_SPACE);

            int right_open = (right_end_x >= 0 && right_end_x < BOARD_SIZE &&
                            right_end_y >= 0 && right_end_y < BOARD_SIZE &&
                            board[right_end_y][right_end_x] == STONE_SPACE);

            if (left_open && right_open) {
                return 1;
            }
        }
    }

    return 0;
}

// 片側空き四連（_●●●● や ●●●●_）を検出
// board[y][x]は空きマス。ここに石を置いたら片側空き四連ができるかチェック
static int has_closed_four(const int board[][BOARD_SIZE], int x, int y, int color) {
    if (board[y][x] != STONE_SPACE) {
        return 0;
    }

    int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

    for (int d = 0; d < 4; d++) {
        int dx = directions[d][0];
        int dy = directions[d][1];

        // 左右の連続する同色の石をカウント
        int left_count = 0;
        for (int i = 1; i <= 3; i++) {
            int nx = x - dx * i;
            int ny = y - dy * i;
            if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board[ny][nx] == color) {
                left_count++;
            } else {
                break;
            }
        }

        int right_count = 0;
        for (int i = 1; i <= 3; i++) {
            int nx = x + dx * i;
            int ny = y + dy * i;
            if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board[ny][nx] == color) {
                right_count++;
            } else {
                break;
            }
        }

        // この位置を含めて4連になるか？（左+1+右 == 4）
        if (left_count + right_count == 3) {
            // 両端の状態をチェック
            int left_end_x = x - dx * (left_count + 1);
            int left_end_y = y - dy * (left_count + 1);
            int right_end_x = x + dx * (right_count + 1);
            int right_end_y = y + dy * (right_count + 1);

            int left_open = (left_end_x >= 0 && left_end_x < BOARD_SIZE &&
                           left_end_y >= 0 && left_end_y < BOARD_SIZE &&
                           board[left_end_y][left_end_x] == STONE_SPACE);

            int right_open = (right_end_x >= 0 && right_end_x < BOARD_SIZE &&
                            right_end_y >= 0 && right_end_y < BOARD_SIZE &&
                            board[right_end_y][right_end_x] == STONE_SPACE);

            // 片側だけが空いている場合（両端空きは除外）
            if ((left_open && !right_open) || (!left_open && right_open)) {
                if (g_config.debug_level >= 2) {
                    printf("[TSS] Closed-four detected at (%d,%d) direction=(%d,%d) left_open=%d right_open=%d\n",
                           x, y, dx, dy, left_open, right_open);
                }
                return 1;
            }
        }
    }

    return 0;
}

// 開いた三（両端が空いている3連で、4連にできる）を検出
// board[y][x]は空きマス。ここに石を置いたら3連両空きができるかチェック
// パターン: _●●●_ (●が3つ連続、両端が空き、この位置がその一部または端)
static int has_open_three(const int board[][BOARD_SIZE], int x, int y, int color) {
    if (board[y][x] != STONE_SPACE) {
        return 0;  // 既に石がある
    }

    int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

    if (g_config.debug_level >= 3) {
        printf("[TSS-DETAIL] Checking open-three at (%d,%d) color=%d\n", x, y, color);
    }

    for (int d = 0; d < 4; d++) {
        int dx = directions[d][0];
        int dy = directions[d][1];

        // この位置に石を置いたときに、3連両空きになるパターンを探す
        // パターン1: _XXX (この位置が左端、右に2個の石)
        // パターン2: X_XX (この位置が2番目、左に1個、右に1個)
        // パターン3: XX_X (この位置が3番目、左に2個、右に1個)
        // パターン4: XXX_ (この位置が右端、左に2個の石)

        // 左右の連続する同色の石をカウント
        int left_count = 0;
        for (int i = 1; i <= 2; i++) {
            int nx = x - dx * i;
            int ny = y - dy * i;
            if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board[ny][nx] == color) {
                left_count++;
            } else {
                break;
            }
        }

        int right_count = 0;
        for (int i = 1; i <= 2; i++) {
            int nx = x + dx * i;
            int ny = y + dy * i;
            if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board[ny][nx] == color) {
                right_count++;
            } else {
                break;
            }
        }

        if (g_config.debug_level >= 3) {
            printf("[TSS-DETAIL]   dir(%d,%d): left=%d, right=%d\n", dx, dy, left_count, right_count);
        }

        // この位置を含めて3連になるか？（左+1+右 == 3）
        if (left_count + right_count == 2) {
            // 3連になる可能性がある。両端が空いているかチェック

            // 左端の位置
            int left_end_x = x - dx * (left_count + 1);
            int left_end_y = y - dy * (left_count + 1);

            // 右端の位置
            int right_end_x = x + dx * (right_count + 1);
            int right_end_y = y + dy * (right_count + 1);  // 修正: - を + に

            int left_open = (left_end_x >= 0 && left_end_x < BOARD_SIZE &&
                           left_end_y >= 0 && left_end_y < BOARD_SIZE &&
                           board[left_end_y][left_end_x] == STONE_SPACE);

            int right_open = (right_end_x >= 0 && right_end_x < BOARD_SIZE &&
                            right_end_y >= 0 && right_end_y < BOARD_SIZE &&
                            board[right_end_y][right_end_x] == STONE_SPACE);

            if (g_config.debug_level >= 3) {
                printf("[TSS-DETAIL]   left_end(%d,%d)=%s, right_end(%d,%d)=%s\n",
                       left_end_x, left_end_y, left_open ? "OPEN" : "BLOCKED",
                       right_end_x, right_end_y, right_open ? "OPEN" : "BLOCKED");
            }

            if (left_open && right_open) {
                const char *dir_name = "";
                if (dx == 1 && dy == 0) dir_name = "horizontal (→)";
                else if (dx == 0 && dy == 1) dir_name = "vertical (↓)";
                else if (dx == 1 && dy == 1) dir_name = "diagonal \\ (↘)";
                else if (dx == 1 && dy == -1) dir_name = "diagonal / (↗)";

                printf("[TSS] ★★★ OPEN-THREE at (%d,%d) %s: L=%d R=%d ★★★\n",
                       x, y, dir_name, left_count, right_count);
                return 1;
            }
        }
    }

    return 0;
}

// 間空き三（●●_● や ●_●● など）を検出
// board[y][x]は空きマス。ここに石を置いたら間空き三ができるかチェック
// _●●_●_ または _●_●●_ のパターンを検出
static __attribute__((unused)) int has_split_three(const int board[][BOARD_SIZE], int x, int y, int color) {
    if (board[y][x] != STONE_SPACE) {
        return 0;
    }

    int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
    int found = 0;

    for (int d = 0; d < 4; d++) {
        int dx = directions[d][0];
        int dy = directions[d][1];

        // パターン1: _●●_●_ を検出
        // (x,y)が中央の●の場合: 左に●1つ、左2つ目に空き、左3つ目に●、左4つ目に空き、右に空き
        // (x,y)が左の●の場合: 右に●1つ、右2つ目に空き、右3つ目に●、右4つ目に空き、左に空き
        // (x,y)が右の●の場合: 左に空き、左2つ目に●、左3つ目に空き、左4つ目に●1つ、左5つ目に空き

        // パターン: _●●_●_ のチェック (x,y)が2番目の●の場合
        // 左に●、左左に空き、右に空き、右右に●、右右右に空き
        int lx1 = x - dx, ly1 = y - dy;         // 左1
        int lx2 = x - dx*2, ly2 = y - dy*2;     // 左2
        int rx1 = x + dx, ry1 = y + dy;         // 右1
        int rx2 = x + dx*2, ry2 = y + dy*2;     // 右2
        int rx3 = x + dx*3, ry3 = y + dy*3;     // 右3

        if (lx1 >= 0 && lx1 < BOARD_SIZE && ly1 >= 0 && ly1 < BOARD_SIZE &&
            lx2 >= 0 && lx2 < BOARD_SIZE && ly2 >= 0 && ly2 < BOARD_SIZE &&
            rx1 >= 0 && rx1 < BOARD_SIZE && ry1 >= 0 && ry1 < BOARD_SIZE &&
            rx2 >= 0 && rx2 < BOARD_SIZE && ry2 >= 0 && ry2 < BOARD_SIZE &&
            rx3 >= 0 && rx3 < BOARD_SIZE && ry3 >= 0 && ry3 < BOARD_SIZE) {

            // _●●_●_
            if (board[ly2][lx2] == STONE_SPACE && board[ly1][lx1] == color &&
                board[ry1][rx1] == STONE_SPACE && board[ry2][rx2] == color &&
                board[ry3][rx3] == STONE_SPACE) {
                if (g_config.debug_level >= 2) {
                    printf("[TSS] Split-three detected (pattern: _●●_●_) at (%d,%d), direction=(%d,%d)\n",
                           x, y, dx, dy);
                }
                found = 1;
                break;
            }
        }

        if (found) break;

        // パターン: _●_●●_ のチェック (x,y)が4番目の●の場合
        // 左に●、左左に空き、左左左に●、左左左左に空き、右に空き
        int lx3 = x - dx*3, ly3 = y - dy*3;     // 左3
        int lx4 = x - dx*4, ly4 = y - dy*4;     // 左4

        if (lx1 >= 0 && lx1 < BOARD_SIZE && ly1 >= 0 && ly1 < BOARD_SIZE &&
            lx2 >= 0 && lx2 < BOARD_SIZE && ly2 >= 0 && ly2 < BOARD_SIZE &&
            lx3 >= 0 && lx3 < BOARD_SIZE && ly3 >= 0 && ly3 < BOARD_SIZE &&
            lx4 >= 0 && lx4 < BOARD_SIZE && ly4 >= 0 && ly4 < BOARD_SIZE &&
            rx1 >= 0 && rx1 < BOARD_SIZE && ry1 >= 0 && ry1 < BOARD_SIZE) {

            // _●_●●_
            if (board[ly4][lx4] == STONE_SPACE && board[ly3][lx3] == color &&
                board[ly2][lx2] == STONE_SPACE && board[ly1][lx1] == color &&
                board[ry1][rx1] == STONE_SPACE) {
                if (g_config.debug_level >= 2) {
                    printf("[TSS] Split-three detected (pattern: _●_●●_) at (%d,%d), direction=(%d,%d)\n",
                           x, y, dx, dy);
                }
                found = 1;
                break;
            }
        }

        if (found) break;

        // パターン: _●●_●_ のチェック (x,y)が4番目の●の場合
        if (lx1 >= 0 && lx1 < BOARD_SIZE && ly1 >= 0 && ly1 < BOARD_SIZE &&
            lx2 >= 0 && lx2 < BOARD_SIZE && ly2 >= 0 && ly2 < BOARD_SIZE &&
            lx3 >= 0 && lx3 < BOARD_SIZE && ly3 >= 0 && ly3 < BOARD_SIZE &&
            lx4 >= 0 && lx4 < BOARD_SIZE && ly4 >= 0 && ly4 < BOARD_SIZE &&
            rx1 >= 0 && rx1 < BOARD_SIZE && ry1 >= 0 && ry1 < BOARD_SIZE) {

            // _●●_●_
            if (board[ly4][lx4] == STONE_SPACE && board[ly3][lx3] == color &&
                board[ly2][lx2] == color && board[ly1][lx1] == STONE_SPACE &&
                board[ry1][rx1] == STONE_SPACE) {
                if (g_config.debug_level >= 2) {
                    printf("[TSS] Split-three detected (pattern: _●●_●_) at (%d,%d), direction=(%d,%d)\n",
                           x, y, dx, dy);
                }
                found = 1;
                break;
            }
        }

        if (found) break;
    }

    return found;
}

// 間空き四（●●_●● のようなパターン）を検出
// 注: 現在未使用だが、将来的な拡張のために保持
static __attribute__((unused)) int has_split_four(const int board[][BOARD_SIZE], int x, int y, int color) {
    int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

    for (int d = 0; d < 4; d++) {
        int dx = directions[d][0];
        int dy = directions[d][1];

        // ●●_●● のパターンをチェック（中央に空きがある）
        for (int gap_pos = 1; gap_pos <= 3; gap_pos++) {
            int stone_count = 0;
            int valid = 1;

            for (int i = -2; i <= 2; i++) {
                if (i == 0) continue;  // 配置予定地はスキップ

                int nx = x + dx * i;
                int ny = y + dy * i;

                if (nx < 0 || nx >= BOARD_SIZE || ny < 0 || ny >= BOARD_SIZE) {
                    valid = 0;
                    break;
                }

                if (i == gap_pos || i == -gap_pos) {
                    // ギャップ位置は空きであるべき
                    if (board[ny][nx] != STONE_SPACE) {
                        valid = 0;
                        break;
                    }
                } else {
                    // ギャップ以外は自分の石
                    if (board[ny][nx] == color) {
                        stone_count++;
                    } else if (board[ny][nx] != STONE_SPACE) {
                        valid = 0;
                        break;
                    }
                }
            }

            if (valid && stone_count >= 3) {
                return 1;
            }
        }
    }

    return 0;
}

// 脅威候補の構造体
typedef struct {
    int x;
    int y;
    int priority;  // 0=開いた四, 1=開いた三, 2=間空き三
    int distance_to_center;  // 中央からの距離（同じ優先度の場合の判断材料）
} ThreatCandidate;

// 中央からの距離を計算（マンハッタン距離）
static __attribute__((unused)) int distance_from_center(int x, int y) {
    int center = BOARD_SIZE / 2;
    return abs(x - center) + abs(y - center);
}

// 脅威候補を比較（優先度順、同じ優先度なら中央に近い方）
static __attribute__((unused)) int compare_threats(const void *a, const void *b) {
    const ThreatCandidate *ta = (const ThreatCandidate *)a;
    const ThreatCandidate *tb = (const ThreatCandidate *)b;

    // 優先度が異なる場合：小さい方が優先（0=開いた四が最優先）
    if (ta->priority != tb->priority) {
        return ta->priority - tb->priority;
    }

    // 優先度が同じ場合：中央に近い方を優先
    return ta->distance_to_center - tb->distance_to_center;
}

// 開いた四のみを探す（開いた三は探索しない - 過剰防御防止）
int find_strong_attack_move(const int board[][BOARD_SIZE], int color, int *attack_x, int *attack_y) {
    if (g_config.debug_level >= 2) {
        printf("[TSS] Searching for OPEN-FOUR only (color=%d)\n", color);
    }

    // 開いた四のみを探す
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (board[y][x] == STONE_SPACE) {
                // 禁じ手チェック（黒の場合）
                if (color == STONE_BLACK && is_forbidden(board, x, y, color)) {
                    continue;
                }

                // 開いた四をチェック
                if (has_open_four((const int(*)[BOARD_SIZE])board, x, y, color)) {
                    *attack_x = x;
                    *attack_y = y;
                    if (g_config.debug_level >= 1) {
                        printf("[TSS-OpenFour] Open-four attack found at (%d, %d)\n", x, y);
                    }
                    return 2;  // 開いた四
                }
            }
        }
    }

    if (g_config.debug_level >= 2) {
        printf("[TSS] No open-four found\n");
    }
    return 0;  // 見つからず
}

// 相手の開いた四・開いた三を防ぐ手を探す（深さ探索で最善手を選択）
int find_strong_defense_move(const int board[][BOARD_SIZE], int color, int *defense_x, int *defense_y) {
    int opponent = (color == STONE_BLACK) ? STONE_WHITE : STONE_BLACK;

    if (g_config.debug_level >= 2) {
        printf("[TSS-Defense] Searching for defense moves (opponent=%d)\n", opponent);
    }

    // ステップ1: 相手の脅威をすべて収集
    typedef struct {
        int x, y;
        int threat_level;  // 2=片側空き四, 3=開いた四（開いた三は対象外）
        int threat_count;  // この位置が防ぐ脅威の数
    } DefenseCandidate;

    DefenseCandidate candidates[BOARD_SIZE * BOARD_SIZE];
    int candidate_count = 0;

    // 相手の開いた四と片側空き四と開いた三をすべて探す
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (board[y][x] == STONE_SPACE) {
                // 禁じ手チェック（自分が黒の場合）
                if (color == STONE_BLACK && is_forbidden(board, x, y, color)) {
                    if (g_config.debug_level >= 2) {
                        printf("[TSS-Defense] Position (%d,%d) is forbidden, skipping\n", x, y);
                    }
                    continue;
                }

                int max_threat = 0;
                int total_threats = 0;

                // 相手の開いた四をチェック（最優先）
                if (has_open_four((const int(*)[BOARD_SIZE])board, x, y, opponent)) {
                    max_threat = 3;
                    total_threats++;
                    if (g_config.debug_level >= 2) {
                        printf("[TSS] Position (%d,%d) blocks OPEN-FOUR\n", x, y);
                    }
                }

                // 相手の片側空き四をチェック（高優先度）
                if (has_closed_four((const int(*)[BOARD_SIZE])board, x, y, opponent)) {
                    if (max_threat < 2) max_threat = 2;
                    total_threats++;
                    if (g_config.debug_level >= 2) {
                        printf("[TSS] Position (%d,%d) blocks CLOSED-FOUR\n", x, y);
                    }
                }

                // 相手の開いた三をチェック（中優先度）
                // 開いた三は次に四連にされると防げなくなるため、防御が必要
                if (has_open_three((const int(*)[BOARD_SIZE])board, x, y, opponent)) {
                    if (max_threat < 1) max_threat = 1;
                    total_threats++;
                    if (g_config.debug_level >= 2) {
                        printf("[TSS] Position (%d,%d) blocks OPEN-THREE\n", x, y);
                    }
                }

                // この位置が脅威を防ぐ場合、候補に追加
                if (max_threat >= 1) {  // 開いた三以上を防御対象に
                    candidates[candidate_count].x = x;
                    candidates[candidate_count].y = y;
                    candidates[candidate_count].threat_level = max_threat;
                    candidates[candidate_count].threat_count = total_threats;
                    candidate_count++;

                    if (g_config.debug_level >= 2) {
                        const char *threat_name = (max_threat == 3) ? "OPEN-FOUR" :
                                                 (max_threat == 2) ? "CLOSED-FOUR" : "OPEN-THREE";
                        printf("[TSS] Defense candidate %d: (%d,%d) %s (blocks %d threat%s)\n",
                               candidate_count, x, y, threat_name, total_threats,
                               total_threats > 1 ? "s" : "");
                    }
                }
            }
        }
    }

    // 防御候補が見つからなければ終了
    if (candidate_count == 0) {
        if (g_config.debug_level >= 2) {
            printf("[TSS] No defense required\n");
        }
        return 0;
    }

    // ステップ2: 候補が1つだけなら、それを返す
    if (candidate_count == 1) {
        *defense_x = candidates[0].x;
        *defense_y = candidates[0].y;
        if (g_config.debug_level >= 1) {
            const char *threat_name = (candidates[0].threat_level == 3) ? "OPEN-FOUR" : "CLOSED-FOUR";
            printf("[TSS-Defense] Single defense candidate at (%d,%d) - %s\n",
                   *defense_x, *defense_y, threat_name);
        }
        return candidates[0].threat_level;
    }

// ステップ3: 複数の候補がある場合、脅威数と脅威レベルで選択
if (g_config.debug_level >= 1) {
    printf("[TSS-Defense] Evaluating %d defense candidates...\n", candidate_count);
}

// 【重要な修正】複数のOPEN-THREEを検出
int open_three_count = 0;
for (int i = 0; i < candidate_count; i++) {
    if (candidates[i].threat_level == 1) {  // OPEN-THREE
        open_three_count++;
    }
}

// 優先順位の方針:
// 1. 複数のOPEN-THREE（次ターンで確実に負ける）→ 最優先
// 2. OPEN-FOUR（次ターンで確実に負ける）→ 最優先
// 3. 複数脅威を防ぐ位置（threat_count >= 2）→ 高優先
// 4. CLOSED-FOUR → 中優先
// 5. 単一OPEN-THREE → 低優先

// まず最多脅威数を探す
int max_threat_count = 0;
for (int i = 0; i < candidate_count; i++) {
    if (candidates[i].threat_count > max_threat_count) {
        max_threat_count = candidates[i].threat_count;
    }
}

// 【修正】複数のOPEN-THREEがある場合は緊急事態
bool multiple_open_three_crisis = (open_three_count >= 2);

if (multiple_open_three_crisis && g_config.debug_level >= 1) {
    printf("[TSS-Defense] ⚠️  CRITICAL: Multiple OPEN-THREE detected (%d)! Immediate defense required!\n",
           open_three_count);
}

// 評価対象を決定
int best_score = -999999;
int best_idx = 0;
int found = 0;

if (g_config.debug_level >= 2) {
    printf("[TSS-Defense] Priority: max_threats=%d, open_three_count=%d, crisis=%d\n",
           max_threat_count, open_three_count, multiple_open_three_crisis);
}

for (int i = 0; i < candidate_count; i++) {
    // 【修正】複数OPEN-THREE危機の場合
    if (multiple_open_three_crisis) {
        // OPEN-THREEを防ぐ手のみを評価対象にする
        if (candidates[i].threat_level != 1) {
            continue;
        }
    }
    // 優先度チェック（通常時）
    else if (max_threat_count >= 2) {
        // 複数脅威を防ぐ候補がある場合は、それらのみ評価
        if (candidates[i].threat_count < max_threat_count) {
            continue;
        }
    } else {
        // 単一脅威の場合は、開いた四（レベル3）を優先
        if (candidates[i].threat_level < 3) {
            // 開いた四がない場合は全候補を評価
            int has_open_four = 0;
            for (int j = 0; j < candidate_count; j++) {
                if (candidates[j].threat_level == 3) {
                    has_open_four = 1;
                    break;
                }
            }
            if (has_open_four) {
                continue;
            }
        }
    }
        int x = candidates[i].x;
        int y = candidates[i].y;

        // 仮に石を置いて評価
        int temp_board[BOARD_SIZE][BOARD_SIZE];
        memcpy(temp_board, board, sizeof(temp_board));
        temp_board[y][x] = color;

        // 評価値を計算（自分の視点で）
        int score = evaluate_position((const int(*)[BOARD_SIZE])temp_board, x, y, color);

        // 相手の視点でも評価（防御の完全性）
        temp_board[y][x] = opponent;
        int opponent_score = evaluate_position((const int(*)[BOARD_SIZE])temp_board, x, y, opponent);

        // 総合評価: 自分の得点 - 相手の得点（防御を3倍重視）
        // 脅威数が多い場合は、防御をさらに重視
        int defense_weight = 2 + candidates[i].threat_count;
        int total_score = score - opponent_score * defense_weight;

        if (g_config.debug_level >= 2) {
            printf("[TSS]   Candidate %d: (%d,%d) threat_level=%d threats=%d score=%d (self=%d, opp=%d)\n",
                   i, x, y, candidates[i].threat_level, candidates[i].threat_count,
                   total_score, score, opponent_score);
        }

        if (!found || total_score > best_score) {
            best_score = total_score;
            best_idx = i;
            found = 1;
        }
    }

    *defense_x = candidates[best_idx].x;
    *defense_y = candidates[best_idx].y;

    if (g_config.debug_level >= 1) {
        const char *threat_name = (candidates[best_idx].threat_level == 3) ? "OPEN-FOUR" :
                                 (candidates[best_idx].threat_level == 2) ? "CLOSED-FOUR" : "OPEN-THREE";
        printf("[TSS-Defense] Best defense selected: (%d,%d) %s (blocks %d threat%s) score=%d (from %d candidates)\n",
               *defense_x, *defense_y, threat_name, candidates[best_idx].threat_count,
               candidates[best_idx].threat_count > 1 ? "s" : "", best_score, candidate_count);
    }

    return candidates[best_idx].threat_level;
}

// ========================================
// 四連防御（開いた四と片側空き四の統合検出）
// ========================================

// 相手の四連（開いた四または片側空き四）を防ぐ手を探す
// 戻り値: 0=見つからず, 1=片側空き四を防御, 2=開いた四を防御
int find_four_defense_move(const int board[][BOARD_SIZE], int color, int *defense_x, int *defense_y) {
    int opponent = (color == STONE_BLACK) ? STONE_WHITE : STONE_BLACK;

    if (g_config.debug_level >= 2) {
        printf("[TSS-FourDefense] Searching for four-threats (opponent=%d)\n", opponent);
    }

    typedef struct {
        int x, y;
        int threat_type;  // 1=片側空き四, 2=開いた四
    } FourCandidate;

    FourCandidate candidates[BOARD_SIZE * BOARD_SIZE];
    int candidate_count = 0;

    // 相手の四連をすべて探す
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (board[y][x] == STONE_SPACE) {
                // 禁じ手チェック（自分が黒の場合）
                if (color == STONE_BLACK && is_forbidden(board, x, y, color)) {
                    continue;
                }

                // 開いた四をチェック（最優先）
                if (has_open_four((const int(*)[BOARD_SIZE])board, x, y, opponent)) {
                    candidates[candidate_count].x = x;
                    candidates[candidate_count].y = y;
                    candidates[candidate_count].threat_type = 2;
                    candidate_count++;

                    if (g_config.debug_level >= 1) {
                        printf("[TSS-FourDefense] OPEN-FOUR threat at (%d,%d)\n", x, y);
                    }
                }
                // 片側空き四をチェック
                else if (has_closed_four((const int(*)[BOARD_SIZE])board, x, y, opponent)) {
                    candidates[candidate_count].x = x;
                    candidates[candidate_count].y = y;
                    candidates[candidate_count].threat_type = 1;
                    candidate_count++;

                    if (g_config.debug_level >= 1) {
                        printf("[TSS-FourDefense] CLOSED-FOUR threat at (%d,%d)\n", x, y);
                    }
                }
            }
        }
    }

    if (candidate_count == 0) {
        if (g_config.debug_level >= 2) {
            printf("[TSS-FourDefense] No four-threats found\n");
        }
        return 0;
    }

    // 複数の脅威がある場合は、開いた四を優先
    int best_idx = 0;
    int best_threat = candidates[0].threat_type;

    for (int i = 1; i < candidate_count; i++) {
        if (candidates[i].threat_type > best_threat) {
            best_threat = candidates[i].threat_type;
            best_idx = i;
        }
    }

    *defense_x = candidates[best_idx].x;
    *defense_y = candidates[best_idx].y;

    if (g_config.debug_level >= 1) {
        const char *type_str = (candidates[best_idx].threat_type == 2) ? "OPEN-FOUR" : "CLOSED-FOUR";
        printf("[TSS-FourDefense] Defending %s at (%d,%d)\n", type_str, *defense_x, *defense_y);
    }

    return candidates[best_idx].threat_type;
}

// ========================================
// 攻撃的な四連・三連を作る手を探す（防御前の攻撃チャンス）
// ========================================

// 自分の四連（FOUR）または攻撃的な三連（OPEN_THREE）を作る手を探す
// 戻り値: 0=なし, 1=四連（FOUR）, 2=複数の開いた三
int find_aggressive_attack_move(const int board[][BOARD_SIZE], int color, int *attack_x, int *attack_y) {
    if (g_config.debug_level >= 2) {
        printf("[TSS-AggressiveAttack] Searching for aggressive moves (color=%d)\n", color);
    }

    typedef struct {
        int x, y;
        int attack_value;  // 攻撃価値（高いほど強力）
        int four_count;    // 四連の数
        int open_three_count;  // 開いた三の数
    } AttackCandidate;

    AttackCandidate candidates[BOARD_SIZE * BOARD_SIZE];
    int candidate_count = 0;

    // すべての空きマスをチェック
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (board[y][x] == STONE_SPACE) {
                int four_count = 0;
                int open_three_count = 0;

                // 一時的に石を置いてチェック
                int temp_board[BOARD_SIZE][BOARD_SIZE];
                memcpy(temp_board, board, sizeof(temp_board));
                temp_board[y][x] = color;

                // 4方向をチェック
                int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

                for (int d = 0; d < 4; d++) {
                    int dx = directions[d][0];
                    int dy = directions[d][1];
                    int count = 1;  // 配置した石を含む

                    // 正方向をカウント
                    int nx = x + dx;
                    int ny = y + dy;
                    while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE &&
                           temp_board[ny][nx] == color) {
                        count++;
                        nx += dx;
                        ny += dy;
                    }
                    int right_open = (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE &&
                                     temp_board[ny][nx] == STONE_SPACE);

                    // 逆方向をカウント
                    nx = x - dx;
                    ny = y - dy;
                    while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE &&
                           temp_board[ny][nx] == color) {
                        count++;
                        nx -= dx;
                        ny -= dy;
                    }
                    int left_open = (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE &&
                                    temp_board[ny][nx] == STONE_SPACE);

                    // 四連（片側空き）をカウント
                    if (count == 4 && (left_open || right_open)) {
                        four_count++;
                    }

                    // 開いた三（両端空き）をカウント
                    if (count == 3 && left_open && right_open) {
                        open_three_count++;
                    }
                }

                // 攻撃価値を計算
                // 四連: 100点
                // 開いた三: 30点（2個以上で価値が高い）
                int attack_value = four_count * 100 + open_three_count * 30;

                // 四連が1つ以上、または開いた三が2つ以上ある場合は候補に追加
                if (four_count >= 1 || open_three_count >= 2) {
                    // 【重要】禁じ手チェック（黒の場合）
                    // .libgomoku_to.c の正確な判定を使用
                    if (color == STONE_BLACK && is_forbidden(board, x, y, color)) {
                        if (g_config.debug_level >= 2) {
                            printf("[TSS-AggressiveAttack] Candidate at (%d,%d) is FORBIDDEN! Skipping...\n", x, y);
                        }
                        continue;  // この候補をスキップ
                    }

                    candidates[candidate_count].x = x;
                    candidates[candidate_count].y = y;
                    candidates[candidate_count].attack_value = attack_value;
                    candidates[candidate_count].four_count = four_count;
                    candidates[candidate_count].open_three_count = open_three_count;
                    candidate_count++;

                    if (g_config.debug_level >= 2) {
                        printf("[TSS-AggressiveAttack] Candidate at (%d,%d): FOUR=%d, OPEN_THREE=%d, value=%d\n",
                               x, y, four_count, open_three_count, attack_value);
                    }
                }
            }
        }
    }

    if (candidate_count == 0) {
        if (g_config.debug_level >= 2) {
            printf("[TSS-AggressiveAttack] No aggressive moves found\n");
        }
        return 0;
    }

    // 最も攻撃価値の高い手を選択
    int best_idx = 0;
    int best_value = candidates[0].attack_value;

    for (int i = 1; i < candidate_count; i++) {
        if (candidates[i].attack_value > best_value) {
            best_value = candidates[i].attack_value;
            best_idx = i;
        }
    }

    *attack_x = candidates[best_idx].x;
    *attack_y = candidates[best_idx].y;

    // 【最終確認】選ばれた手が禁じ手でないことを再確認
    if (color == STONE_BLACK && is_forbidden(board, *attack_x, *attack_y, color)) {
        if (g_config.debug_level >= 1) {
            printf("[TSS-AggressiveAttack] ERROR: Best candidate at (%d,%d) is FORBIDDEN!\n",
                   *attack_x, *attack_y);
        }
        return 0;  // 禁じ手なので攻撃手なしとして返す
    }

    if (g_config.debug_level >= 1) {
        printf("[TSS-AggressiveAttack] Best attack: (%d,%d) FOUR=%d, OPEN_THREE=%d, value=%d (LEGAL)\n",
               *attack_x, *attack_y,
               candidates[best_idx].four_count,
               candidates[best_idx].open_three_count,
               candidates[best_idx].attack_value);
    }

    // 四連がある場合は1を返す、開いた三のみの場合は2を返す
    return (candidates[best_idx].four_count >= 1) ? 1 : 2;
}
