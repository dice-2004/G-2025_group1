#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "..\gomoku.h"
#include "main_engine.h"

#define WIN_SCORE 1000000000
#define LOSE_SCORE -100000

/*評価関数*/
// 重み（調整可能）
#define W5 1000000000
#define W_OPEN4 50000
#define W_JUMP4 30000
#define W_CLOSED4 5000
#define W_OPEN3 1000
#define W_JUMP3 500
#define W_OPEN2 100

static inline int other_side(int side) {
	return (side == STONE_BLACK) ? STONE_WHITE : STONE_BLACK;
}

static inline bool in_board(int x, int y) {
	return x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE;
}

extern int lenCheck(int board[][BOARD_SIZE], int x, int y);

// 1 方向 (dx,dy) に沿って中心 (x,y) を含む長さ WINDOW のウィンドウをスライドして
// サブウィンドウでパターンを検出する。WINDOW=8 に合わせる。
#define WINDOW 8
#define HALF (WINDOW - 1)

static int eval_dir(int board[][BOARD_SIZE], int x, int y, int dx, int dy, int side) {
	int base_x = x - HALF * dx;
	int base_y = y - HALF * dy;
	int other = other_side(side);
	int score = 0;

	int window[WINDOW];

	for (int off = 0; off <= 2 * HALF; off++) {
		int sx = base_x + off * dx;
		int sy = base_y + off * dy;
		bool ok = true;
		for (int k = 0; k < WINDOW; k++) {
			int nx = sx + k * dx;
			int ny = sy + k * dy;
			if (!in_board(nx, ny)) { ok = false; break; }
			window[k] = board[ny][nx];
		}
		if (!ok) continue;

		// サブウィンドウ長で連続パターンを検出（5,4,3,2）
		for (int klen = 5; klen >= 2; klen--) {
			for (int p = 0; p <= WINDOW - klen; p++) {
				int own = 0, opp = 0;
				for (int t = 0; t < klen; t++) {
					if (window[p + t] == side) own++;
					else if (window[p + t] == other) opp++;
				}
				if (opp > 0 && own == 0) continue;
				if (own == klen && opp == 0) {
					if (klen == 5) return W5;
					if (klen == 4) {
						int left = p - 1;
						int right = p + klen;
						bool l_empty = (left >= 0) && (window[left] == STONE_SPACE);
						bool r_empty = (right < WINDOW) && (window[right] == STONE_SPACE);
						if (l_empty && r_empty) score += W_OPEN4;
						else if (l_empty || r_empty) score += W_CLOSED4;
						else score += W_CLOSED4 / 2;
					} else if (klen == 3) {
						int left = p - 1;
						int right = p + klen;
						bool l_empty = (left >= 0) && (window[left] == STONE_SPACE);
						bool r_empty = (right < WINDOW) && (window[right] == STONE_SPACE);
						if (l_empty && r_empty) score += W_OPEN3;
						else if (l_empty || r_empty) score += W_JUMP3;
					} else if (klen == 2) {
						int left = p - 1;
						int right = p + klen;
						bool l_empty = (left >= 0) && (window[left] == STONE_SPACE);
						bool r_empty = (right < WINDOW) && (window[right] == STONE_SPACE);
						if (l_empty && r_empty) score += W_OPEN2;
					}
				}
			}
		}

		// jump パターン（subwindow 長 5 で内部に空があるパターン）
		for (int p = 0; p <= WINDOW - 5; p++) {
			int own = 0, opp = 0, emp = 0;
			for (int t = 0; t < 5; t++) {
				if (window[p + t] == side) own++;
				else if (window[p + t] == other) opp++;
				else emp++;
			}
			if (opp == 0 && own == 4 && emp == 1) score += W_JUMP4;
			if (opp == 0 && own == 3 && emp == 2) score += W_JUMP3;
		}
	}

	return score;
}

int evaluate_board(int board[][BOARD_SIZE], int side) {
	int other = other_side(side);
	long long self_score = 0, opp_score = 0;

	// immediate 5 check
	for (int y = 0; y < BOARD_SIZE; y++) {
		for (int x = 0; x < BOARD_SIZE; x++) {
			if (board[y][x] == STONE_SPACE) continue;
			if (lenCheck(board, x, y)) {
				if (board[y][x] == side) return W5;
				else return -W5;
			}
		}
	}

	int dirs[4][2] = {{1,0},{0,1},{1,1},{1,-1}};
	for (int y = 0; y < BOARD_SIZE; y++) {
		for (int x = 0; x < BOARD_SIZE; x++) {
			if (board[y][x] == side) {
				for (int d = 0; d < 4; d++) self_score += eval_dir(board, x, y, dirs[d][0], dirs[d][1], side);
			} else if (board[y][x] == other) {
				for (int d = 0; d < 4; d++) opp_score += eval_dir(board, x, y, dirs[d][0], dirs[d][1], other);
			}
		}
	}

	long long total = self_score - opp_score;
	if (total > INT_MAX) return INT_MAX;
	if (total < INT_MIN) return INT_MIN;
	return (int)total;
}

/*αβ探索*/
/*反復深化*/