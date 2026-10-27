// 開局ブック: 序盤の局面 → 事前に長考で求めた最善手の表 (engine/book/ で生成、book_data.c に埋め込み)
#ifndef G2_BOOK_H
#define G2_BOOK_H

#include <stdint.h>

#include "board.h"

typedef struct {
    uint64_t key;       // 正規形 (8 対称のうちハッシュ最小) の局面キー。表はこの昇順
    uint8_t x, y;       // 正規形の座標での最善手
    int16_t eval;       // 生成時の評価 (手番側から、参考値)
} BookEntry;

// 手番側の手をブックから引く。ヒットしたら 1 を返して *move に盤上の pos を入れる。
// 15 路だけ。空点でない手・黒の禁じ手、どちらかに五・達四の点がある局面では使わない (0)
int book_probe(Board *b, int *move);
void book_set_enabled(int on);      // 既定は有効 (Gomocup の INFO book 0 で無効)
int book_enabled(void);

// 盤面の正規形キーと、それを与える変換 (テスト用)
uint64_t book_key(const Board *b, int *transform);

#endif
