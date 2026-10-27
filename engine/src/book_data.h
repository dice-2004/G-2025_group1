// 開局ブックのデータ (book_data.c は engine/book/build_book.py が生成する)
#ifndef G2_BOOK_DATA_H
#define G2_BOOK_DATA_H

#include "book.h"

extern const BookEntry g_book_renju[];
extern const int g_book_renju_n;
extern const BookEntry g_book_freestyle[];
extern const int g_book_freestyle_n;
extern const int g_book_max_stones;     // 表にある局面の石数の最大

#endif
