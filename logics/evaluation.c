#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "evaluation.h"
#include "../gomoku.h"

// グローバルなパターンスコア（CSVから読み込み可能、学習モジュールから変更可能）
int g_score_five = SCORE_FIVE;
int g_score_open_four = SCORE_OPEN_FOUR;
int g_score_four_four = SCORE_FOUR_FOUR;
int g_score_four_three = SCORE_FOUR_THREE;
int g_score_four = SCORE_FOUR;
int g_score_broken_four = SCORE_BROKEN_FOUR;
int g_score_open_three_three = SCORE_OPEN_THREE_THREE;
int g_score_open_three = SCORE_OPEN_THREE;
int g_score_broken_three = SCORE_BROKEN_THREE;
int g_score_three_three = SCORE_THREE_THREE;
int g_score_three = SCORE_THREE;
int g_score_open_two_two = SCORE_OPEN_TWO_TWO;
int g_score_open_two = SCORE_OPEN_TWO;
int g_score_broken_two = SCORE_BROKEN_TWO;
int g_score_two_two = SCORE_TWO_TWO;
int g_score_two = SCORE_TWO;
int g_score_one = SCORE_ONE;
int g_score_center = SCORE_CENTER;
int g_score_near_enemy = SCORE_NEAR_ENEMY;
int g_score_connection = SCORE_CONNECTION;

// デフォルトスコアを使用
void use_default_pattern_scores(void) {
    g_score_five = SCORE_FIVE;
    g_score_open_four = SCORE_OPEN_FOUR;
    g_score_four_four = SCORE_FOUR_FOUR;
    g_score_four_three = SCORE_FOUR_THREE;
    g_score_four = SCORE_FOUR;
    g_score_broken_four = SCORE_BROKEN_FOUR;
    g_score_open_three_three = SCORE_OPEN_THREE_THREE;
    g_score_open_three = SCORE_OPEN_THREE;
    g_score_broken_three = SCORE_BROKEN_THREE;
    g_score_three_three = SCORE_THREE_THREE;
    g_score_three = SCORE_THREE;
    g_score_open_two_two = SCORE_OPEN_TWO_TWO;
    g_score_open_two = SCORE_OPEN_TWO;
    g_score_broken_two = SCORE_BROKEN_TWO;
    g_score_two_two = SCORE_TWO_TWO;
    g_score_two = SCORE_TWO;
    g_score_one = SCORE_ONE;
    g_score_center = SCORE_CENTER;
    g_score_near_enemy = SCORE_NEAR_ENEMY;
    g_score_connection = SCORE_CONNECTION;
}

// パターンスコアをCSVから読み込み
int load_pattern_scores(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("[Patterns] Cannot open %s, using defaults\n", filename);
        use_default_pattern_scores();
        return 0;
    }

    char line[256];
    int loaded = 0;

    while (fgets(line, sizeof(line), fp)) {
        // コメント行と空行をスキップ
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        char name[64];
        int score;
        char desc[128];

        if (sscanf(line, "%[^,],%d,%[^\n]", name, &score, desc) >= 2) {
            if (strcmp(name, "FIVE") == 0) {
                g_score_five = score;
                loaded++;
            } else if (strcmp(name, "OPEN_FOUR") == 0) {
                g_score_open_four = score;
                loaded++;
            } else if (strcmp(name, "FOUR_FOUR") == 0) {
                g_score_four_four = score;
                loaded++;
            } else if (strcmp(name, "FOUR_THREE") == 0) {
                g_score_four_three = score;
                loaded++;
            } else if (strcmp(name, "FOUR") == 0) {
                g_score_four = score;
                loaded++;
            } else if (strcmp(name, "BROKEN_FOUR") == 0) {
                g_score_broken_four = score;
                loaded++;
            } else if (strcmp(name, "OPEN_THREE_THREE") == 0) {
                g_score_open_three_three = score;
                loaded++;
            } else if (strcmp(name, "OPEN_THREE") == 0) {
                g_score_open_three = score;
                loaded++;
            } else if (strcmp(name, "BROKEN_THREE") == 0) {
                g_score_broken_three = score;
                loaded++;
            } else if (strcmp(name, "THREE_THREE") == 0) {
                g_score_three_three = score;
                loaded++;
            } else if (strcmp(name, "THREE") == 0) {
                g_score_three = score;
                loaded++;
            } else if (strcmp(name, "OPEN_TWO_TWO") == 0) {
                g_score_open_two_two = score;
                loaded++;
            } else if (strcmp(name, "OPEN_TWO") == 0) {
                g_score_open_two = score;
                loaded++;
            } else if (strcmp(name, "BROKEN_TWO") == 0) {
                g_score_broken_two = score;
                loaded++;
            } else if (strcmp(name, "TWO_TWO") == 0) {
                g_score_two_two = score;
                loaded++;
            } else if (strcmp(name, "TWO") == 0) {
                g_score_two = score;
                loaded++;
            } else if (strcmp(name, "ONE") == 0) {
                g_score_one = score;
                loaded++;
            } else if (strcmp(name, "CENTER") == 0) {
                g_score_center = score;
                loaded++;
            } else if (strcmp(name, "NEAR_ENEMY") == 0) {
                g_score_near_enemy = score;
                loaded++;
            } else if (strcmp(name, "CONNECTION") == 0) {
                g_score_connection = score;
                loaded++;
            }
        }
    }

    fclose(fp);

    printf("[Patterns] Loaded %d patterns from %s\n", loaded, filename);
    printf("  FIVE=%d, OPEN_FOUR=%d, FOUR=%d, OPEN_THREE=%d\n",
           g_score_five, g_score_open_four, g_score_four, g_score_open_three);

    return loaded;
}

// ========================================
// パターン評価ヘルパー関数
// ========================================

// 特定方向のラインを抽出（最大11マス分）
static void extract_line(const int board[][BOARD_SIZE], int x, int y, int dx, int dy,
                         int line[], int *length, int max_len) {
    int idx = 0;
    int nx = x - dx * 5;  // 5マス戻る（11マス抽出の中心）
    int ny = y - dy * 5;

    for (int i = 0; i < max_len && idx < max_len; i++) {
        if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE) {
            line[idx++] = board[ny][nx];
        } else {
            line[idx++] = -1;  // 盤外
        }
        nx += dx;
        ny += dy;
    }
    *length = idx;
}

// ラインのパターンを評価（9マス窓）
int evaluate_line(const int line[], int length, int my_color) {
    int score = 0;
    int opponent = (my_color == STONE_BLACK) ? STONE_WHITE : STONE_BLACK;

    // 11マス抽出できない場合はスキップ
    if (length < 11) {
        return 0;
    }

    // 【6マス窓でスライディング評価】
    // 11マス抽出（中心から±5マス）、6マス窓をスライド（i = 0～5）
    for (int i = 0; i <= length - 6; i++) {
        int my_count = 0;
        int opp_count = 0;
        int space_count = 0;

        // 6マス窓の内容をカウント
        for (int j = 0; j < 6; j++) {
            if (line[i + j] == my_color) my_count++;
            else if (line[i + j] == opponent) opp_count++;
            else if (line[i + j] == STONE_SPACE) space_count++;
        }

        // 両側の状況をチェック（11マス抽出なので左右の情報あり）
        bool left_open = (i == 0 || line[i - 1] != opponent);
        bool right_open = (i + 6 >= length || line[i + 6] != opponent);

        // 自分のパターン評価（6マス窓）
        if (opp_count == 0) {
            if (my_count >= 6) {
                // 6連以上（長連）は黒番のみ禁じ手
                if (my_color == STONE_BLACK) {
                    // 黒番の場合は長連なので大幅なペナルティ
                    score -= g_score_five;  // -100000点（最悪の手）
                } else {
                    // 白番の場合は6連以上でも勝ち
                    score += g_score_five;
                }
            } else if (my_count == 5) {
                // 5連達成（黒白とも勝ち）
                score += g_score_five;
            } else if (my_count == 4) {
                // 4連パターン（6マス窓なので開いた四を正確に検出可能）
                if (space_count == 2 && line[i] == STONE_SPACE && line[i + 5] == STONE_SPACE) {
                    // [_][●][●][●][●][_] - 完全な開いた四
                    score += g_score_open_four;
                } else if (space_count == 1) {
                    // 飛び四のパターン検出（●●_●●、●_●●●、●●●_●）
                    bool is_broken_four = false;
                    if (line[i] == my_color && line[i+1] == my_color && line[i+2] == STONE_SPACE &&
                        line[i+3] == my_color && line[i+4] == my_color) {
                        is_broken_four = true;  // ●●_●●
                    } else if (line[i] == my_color && line[i+1] == STONE_SPACE &&
                               line[i+2] == my_color && line[i+3] == my_color && line[i+4] == my_color) {
                        is_broken_four = true;  // ●_●●●
                    } else if (line[i] == my_color && line[i+1] == my_color && line[i+2] == my_color &&
                               line[i+3] == STONE_SPACE && line[i+4] == my_color) {
                        is_broken_four = true;  // ●●●_●
                    }

                    if (is_broken_four) {
                        score += g_score_broken_four;  // 飛び四スコア
                    } else if (left_open && right_open) {
                        score += g_score_open_four * 0.9;
                    } else {
                        score += g_score_four;
                    }
                } else if (left_open && right_open) {
                    // その他の開いた四
                    score += g_score_open_four * 0.9;
                } else {
                    // 片側閉じた四
                    score += g_score_four;
                }
            } else if (my_count == 3 && space_count >= 2) {
                // 3連パターン
                bool is_broken_three = false;

                // 飛び三のパターン検出（●●_●、●_●●）
                if (space_count == 2 || space_count == 3) {
                    if (line[i] == my_color && line[i+1] == my_color && line[i+2] == STONE_SPACE && line[i+3] == my_color) {
                        is_broken_three = true;  // ●●_●
                    } else if (line[i] == my_color && line[i+1] == STONE_SPACE && line[i+2] == my_color && line[i+3] == my_color) {
                        is_broken_three = true;  // ●_●●
                    } else if (line[i] == my_color && line[i+1] == STONE_SPACE &&
                               line[i+2] == STONE_SPACE && line[i+3] == my_color && line[i+4] == my_color) {
                        is_broken_three = true;  // ●__●●
                    } else if (line[i] == my_color && line[i+1] == my_color &&
                               line[i+2] == STONE_SPACE && line[i+3] == STONE_SPACE && line[i+4] == my_color) {
                        is_broken_three = true;  // ●●__●
                    }
                }

                if (is_broken_three && left_open && right_open) {
                    score += g_score_broken_three;  // 飛び三スコア
                } else if (left_open && right_open) {
                    score += g_score_open_three;
                } else {
                    score += g_score_three;
                }
            } else if (my_count == 2 && space_count >= 3) {
                // 2連パターン
                bool is_broken_two = false;

                // 飛び二のパターン検出（●_●）
                if (space_count >= 3) {
                    if (line[i] == my_color && line[i+1] == STONE_SPACE && line[i+2] == my_color) {
                        is_broken_two = true;  // ●_●
                    } else if (line[i] == my_color && line[i+1] == STONE_SPACE &&
                               line[i+2] == STONE_SPACE && line[i+3] == my_color) {
                        is_broken_two = true;  // ●__●
                    }
                }

                if (is_broken_two && left_open && right_open) {
                    score += g_score_broken_two;  // 飛び二スコア
                } else if (left_open && right_open) {
                    score += g_score_open_two;
                } else {
                    score += g_score_two;
                }
            } else if (my_count == 1 && space_count >= 4) {
                // 1個のみ
                score += g_score_one;
            }
        }

        // 相手のパターンを防ぐ評価（少し低めに）
        if (my_count == 0) {
            if (opp_count == 5) {
                score -= g_score_five;
            } else if (opp_count == 4 && space_count == 1) {
                if (left_open && right_open) {
                    score -= g_score_open_four * 0.9;
                } else {
                    score -= g_score_four * 0.9;
                }
            } else if (opp_count == 3 && space_count == 2) {
                if (left_open && right_open) {
                    score -= g_score_open_three * 0.8;
                } else {
                    score -= g_score_three * 0.8;
                }
            } else if (opp_count == 2 && space_count == 3) {
                if (left_open && right_open) {
                    score -= g_score_open_two * 0.7;
                } else {
                    score -= g_score_two * 0.7;
                }
            }
        }
    }

    return score;
}

// ========================================
// 盤面評価関数
// ========================================

// 盤面全体を評価
int evaluate_board(const int board[][BOARD_SIZE], int my_color) {
    int total_score = 0;

    // 全マスについて評価
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (board[y][x] == my_color) {
                // 自分の石の周辺を評価
                total_score += evaluate_position(board, x, y, my_color);
            }
        }
    }

    return total_score;
}

// 特定位置の評価（その位置に石を置いた場合のスコア）
int evaluate_position(const int board[][BOARD_SIZE], int x, int y, int color) {
    int score = 0;
    int line[9];
    int length;

    // 4方向を評価
    int directions[4][2] = {
        {1, 0},   // 横
        {0, 1},   // 縦
        {1, 1},   // 右下斜め
        {1, -1}   // 右上斜め
    };

    // 各方向のパターンカウント（複合パターン検出用）
    int four_count = 0;
    int open_three_count = 0;
    int three_count = 0;
    int open_two_count = 0;
    int two_count = 0;

    for (int i = 0; i < 4; i++) {
        extract_line(board, x, y, directions[i][0], directions[i][1], line, &length, 11);
        score += evaluate_line(line, length, color);

        // パターンカウント（複合検出用）
        if (length >= 11) {
            int my_count = 0, space_count = 0, opp_count = 0;
            int opponent = (color == STONE_BLACK) ? STONE_WHITE : STONE_BLACK;

            // 中心付近の6マス窓を分析
            for (int j = 2; j < 8; j++) {
                if (line[j] == color) my_count++;
                else if (line[j] == STONE_SPACE) space_count++;
                else if (line[j] == opponent) opp_count++;
            }

            if (opp_count == 0) {
                if (my_count == 4) four_count++;
                else if (my_count == 3 && space_count >= 2) {
                    // 開いているかチェック
                    bool left_open = (line[1] != opponent && line[1] >= 0);
                    bool right_open = (line[8] != opponent && line[8] >= 0);
                    if (left_open && right_open) open_three_count++;
                    else three_count++;
                } else if (my_count == 2 && space_count >= 3) {
                    bool left_open = (line[1] != opponent && line[1] >= 0);
                    bool right_open = (line[8] != opponent && line[8] >= 0);
                    if (left_open && right_open) open_two_count++;
                    else two_count++;
                }
            }
        }
    }

    // 複合パターンのボーナススコア
    // ⚠️ 重要: 黒番（先手）は四四と三三が禁じ手
    if (four_count >= 2) {
        // 四四（2つ以上の四が交差）
        if (color == STONE_BLACK) {
            // 黒番の場合は禁じ手なので大幅なペナルティ
            score -= g_score_five;  // 最悪の手として評価
        } else {
            // 白番の場合はボーナス（勝ち確定）
            score += g_score_four_four;
        }
    } else if (four_count >= 1 && (three_count >= 1 || open_three_count >= 1)) {
        // 四三（四と三が交差）- これは禁じ手ではない
        score += g_score_four_three;
    }

    if (open_three_count >= 2) {
        // 開いた三×2（三三）
        if (color == STONE_BLACK) {
            // 黒番の場合は禁じ手なので大幅なペナルティ
            score -= g_score_five;  // 最悪の手として評価
        } else {
            // 白番の場合はボーナス（勝ちパターン）
            score += g_score_open_three_three;
        }
    } else if (three_count >= 2 || (three_count >= 1 && open_three_count >= 1)) {
        // 三×2（片側閉じた三が含まれる場合は禁じ手ではない）
        score += g_score_three_three;
    }

    if (open_two_count >= 2) {
        // 開いた二×2
        score += g_score_open_two_two;
    } else if (two_count >= 2 || (two_count >= 1 && open_two_count >= 1)) {
        // 二×2
        score += g_score_two_two;
    }

    // 中央に近いほど少しボーナス（序盤の安定性）- CENTER パターン
    int center_x = BOARD_SIZE / 2;
    int center_y = BOARD_SIZE / 2;
    int dist = abs(x - center_x) + abs(y - center_y);
    score += (BOARD_SIZE - dist) * 2;

    // 中央付近の特別ボーナス
    if (dist <= 2) {
        score += g_score_center;
    }

    // NEAR_ENEMY パターン: 相手の石の近く（牽制・防御）
    int opponent = (color == STONE_BLACK) ? STONE_WHITE : STONE_BLACK;
    int enemy_nearby = 0;
    int friendly_nearby = 0;

    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx;
            int ny = y + dy;
            if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE) {
                if (board[ny][nx] == opponent) {
                    enemy_nearby++;
                } else if (board[ny][nx] == color) {
                    friendly_nearby++;
                }
            }
        }
    }

    // 相手の石の近くにいる場合のボーナス
    if (enemy_nearby > 0) {
        score += g_score_near_enemy * enemy_nearby;
    }

    // CONNECTION パターン: 既存の石との連結
    if (friendly_nearby > 0) {
        score += g_score_connection * friendly_nearby;
    }

    return score;
}

// ========================================
// 深さ1の思考（全ての合法手を評価して最善手を選択）
// ========================================

int find_best_move_simple(const int board[][BOARD_SIZE], int color, int *best_x, int *best_y) {
    Move moves[BOARD_SIZE * BOARD_SIZE];
    int move_count = generate_legal_moves(board, color, moves, BOARD_SIZE * BOARD_SIZE);

    if (move_count == 0) {
        // 合法手がない場合は中央
        *best_x = BOARD_SIZE / 2;
        *best_y = BOARD_SIZE / 2;
        return 0;
    }

    int best_score = -999999;
    int best_idx = 0;

    // 各手を評価
    for (int i = 0; i < move_count; i++) {
        int x = moves[i].x;
        int y = moves[i].y;

        // 仮に石を置いてみる
        int temp_board[BOARD_SIZE][BOARD_SIZE];
        memcpy(temp_board, board, sizeof(temp_board));
        temp_board[y][x] = color;

        // 評価
        int score = evaluate_position(temp_board, x, y, color);

        if (score > best_score) {
            best_score = score;
            best_idx = i;
        }
    }

    *best_x = moves[best_idx].x;
    *best_y = moves[best_idx].y;

    return best_score;
}
