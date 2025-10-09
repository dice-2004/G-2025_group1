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
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include "gomoku.h"
#include "logics/Alpha_Beta.h"
#include "evaluation_function.h"
#include "logics/Iterative_Deepening.h"
#include "logics/Threat_Space_Search.h"


int calcPutPos(int board[][BOARD_SIZE], int com, int *pos_x, int *pos_y)
{
    static int pre[2] = {
        // 直前の石の場所を覚えておくための変数 適当に初期化
        BOARD_SIZE / 2,
        BOARD_SIZE / 2,
    };
    static int flag = 0;
    int count = 0;

    // コンピュータが最初の1手の場合は，(7, 7)に石を置く
    if (flag == 0)
    {
        srand((unsigned int)time(NULL));
        flag = 1;
        if (com == STONE_BLACK)
        {
            *pos_y = BOARD_SIZE / 2;
            *pos_x = BOARD_SIZE / 2;
            return 0;
        }
    }
    while (1)
    {
        //乱数で前に置いた場所の8近傍に置く
        if (count > 5)
        { // 5回置こうとして，どこにも置けなくなった場合はどこでもよいからおいてみる
            *pos_y = (int)(rand() % 15);
            *pos_x = (int)(rand() % 15);
            printf("%d\n", count);
        }
        else
        {
            *pos_y = pre[0] + (int)(rand() % 3 - 1);
            *pos_x = pre[1] + (int)(rand() % 3 - 1);
        }
        if (checkOutPos(*pos_x, *pos_y))
        {
            if (board[*pos_y][*pos_x] == STONE_SPACE)
            {
                break;
            }
            count++;
        }
        else
        { // bug出し
            printf("out of board(%d %d)\n", *pos_x, *pos_y);
            count++;
        }
    }
    pre[0] = *pos_y;
    pre[1] = *pos_x;

    return 0;
}
