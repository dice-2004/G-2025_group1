#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "TSS.h"
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
                    printf("[TSS] Winning move found at (%d, %d)!\n", x, y);
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
        printf("[TSS] Threat detected at (%d, %d), must defend!\n", *threat_x, *threat_y);
        return 1;
    }

    return 0;
}

// ========================================
// 高度な詰み探索（開いた四・開いた三）
// ========================================

// 特定方向に石を数える
static int count_stones_in_direction(const int board[][BOARD_SIZE], int x, int y, int dx, int dy, int color, int *spaces_before_block) {
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
            int right_end_y = y - dy * (right_count + 1);

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
static int has_split_three(const int board[][BOARD_SIZE], int x, int y, int color) {
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
__attribute__((unused))
static int has_split_four(const int board[][BOARD_SIZE], int x, int y, int color) {
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
static int distance_from_center(int x, int y) {
    int center = BOARD_SIZE / 2;
    return abs(x - center) + abs(y - center);
}

// 脅威候補を比較（優先度順、同じ優先度なら中央に近い方）
static int compare_threats(const void *a, const void *b) {
    const ThreatCandidate *ta = (const ThreatCandidate *)a;
    const ThreatCandidate *tb = (const ThreatCandidate *)b;

    // 優先度が異なる場合：小さい方が優先（0=開いた四が最優先）
    if (ta->priority != tb->priority) {
        return ta->priority - tb->priority;
    }

    // 優先度が同じ場合：中央に近い方を優先
    return ta->distance_to_center - tb->distance_to_center;
}

// 自分が開いた四または開いた三を作れる手を探す（すべての候補を評価）
int find_strong_attack_move(const int board[][BOARD_SIZE], int color, int *attack_x, int *attack_y) {
    printf("[TSS] ===== Searching for strong ATTACK moves (color=%d) =====\n", color);

    int empty_count = 0;
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (board[y][x] == STONE_SPACE) empty_count++;
        }
    }
    printf("[TSS] Total empty spaces: %d\n", empty_count);

    // 候補リストを作成（最大でempty_count個）
    ThreatCandidate *candidates = (ThreatCandidate *)malloc(sizeof(ThreatCandidate) * empty_count);
    int candidate_count = 0;

    // Phase 1: すべての空きマスをチェックして候補を収集
    printf("[TSS] Phase 1: Scanning all positions for threats...\n");
    int checked_count = 0;

    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (board[y][x] == STONE_SPACE) {
                // 禁じ手チェック（黒の場合）
                if (color == STONE_BLACK && is_forbidden(board, x, y, color)) {
                    continue;
                }

                checked_count++;
                if (checked_count <= 5 || checked_count % 20 == 0) {
                    printf("[TSS] Checking position (%d,%d)... ", x, y);
                }

                // 開いた四をチェック
                if (has_open_four((const int(*)[BOARD_SIZE])board, x, y, color)) {
                    candidates[candidate_count].x = x;
                    candidates[candidate_count].y = y;
                    candidates[candidate_count].priority = 0;  // 最優先
                    candidates[candidate_count].distance_to_center = distance_from_center(x, y);
                    candidate_count++;
                    if (checked_count <= 5 || checked_count % 20 == 0) {
                        printf("OPEN-FOUR!\n");
                    }
                    continue;
                }

                // 開いた三をチェック
                if (has_open_three((const int(*)[BOARD_SIZE])board, x, y, color)) {
                    candidates[candidate_count].x = x;
                    candidates[candidate_count].y = y;
                    candidates[candidate_count].priority = 1;
                    candidates[candidate_count].distance_to_center = distance_from_center(x, y);
                    candidate_count++;
                    if (checked_count <= 5 || checked_count % 20 == 0) {
                        printf("open-three\n");
                    }
                    continue;
                }

                // 間空き三をチェック
                if (has_split_three((const int(*)[BOARD_SIZE])board, x, y, color)) {
                    candidates[candidate_count].x = x;
                    candidates[candidate_count].y = y;
                    candidates[candidate_count].priority = 2;
                    candidates[candidate_count].distance_to_center = distance_from_center(x, y);
                    candidate_count++;
                    if (checked_count <= 5 || checked_count % 20 == 0) {
                        printf("split-three\n");
                    }
                    continue;
                }

                if (checked_count <= 5 || checked_count % 20 == 0) {
                    printf("no pattern\n");
                }
            }
        }
    }

    printf("[TSS] Total positions checked: %d\n", checked_count);
    printf("[TSS] Total threats found: %d\n", candidate_count);

    // Phase 2: 候補が見つかった場合、最良のものを選択
    if (candidate_count > 0) {
        // 候補をソート（優先度順）
        qsort(candidates, candidate_count, sizeof(ThreatCandidate), compare_threats);

        // 最良の候補を選択
        *attack_x = candidates[0].x;
        *attack_y = candidates[0].y;
        int result_level = (candidates[0].priority == 0) ? 2 : 1;

        const char *threat_type = "";
        if (candidates[0].priority == 0) threat_type = "OPEN-FOUR";
        else if (candidates[0].priority == 1) threat_type = "OPEN-THREE";
        else threat_type = "SPLIT-THREE";

        printf("[TSS] ★★★ Best threat selected: %s at (%d, %d) ★★★\n",
               threat_type, *attack_x, *attack_y);

        // デバッグ: 他の候補も表示（最大5個）
        if (g_config.debug_level >= 2 && candidate_count > 1) {
            printf("[TSS] Other candidates found:\n");
            for (int i = 1; i < candidate_count && i < 6; i++) {
                const char *type = "";
                if (candidates[i].priority == 0) type = "OPEN-FOUR";
                else if (candidates[i].priority == 1) type = "OPEN-THREE";
                else type = "SPLIT-THREE";
                printf("[TSS]   #%d: %s at (%d, %d) dist=%d\n",
                       i+1, type, candidates[i].x, candidates[i].y, candidates[i].distance_to_center);
            }
        }

        free(candidates);
        return result_level;
    }

    free(candidates);
    printf("[TSS] ===== No strong attack move found =====\n\n");
    return 0;  // 見つからず
}

// 相手の開いた四・開いた三を防ぐ手を探す
int find_strong_defense_move(const int board[][BOARD_SIZE], int color, int *defense_x, int *defense_y) {
    int opponent = (color == STONE_BLACK) ? STONE_WHITE : STONE_BLACK;

    printf("[TSS] ===== Searching for DEFENSE moves (blocking opponent color=%d) =====\n", opponent);
    // 相手の強力な攻撃手を探す
    int result = find_strong_attack_move(board, opponent, defense_x, defense_y);
    if (result > 0) {
        printf("[TSS] Defense required at (%d,%d) to block opponent's threat (level=%d)\n",
               *defense_x, *defense_y, result);
    }
    return result;
}
