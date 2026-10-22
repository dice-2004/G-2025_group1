"""ページ内 JS のグローバル状態 (盤の 2 次元配列) を読むサイト向けの共通部分。

サブクラスは BOARD_JS (盤 [row][col] 0/1/2 の配列を返す式)、OVER_JS (終局か)、WINNER_JS (1/2/0)、
PLACE_JS (row, col を打つ関数式) を定義する。座標は Gomocup と同じ x=col, y=row。
"""

import time


class JsStateSite:
    BOARD_JS = "board"
    OVER_JS = "gameOver"
    WINNER_JS = "winner"
    PLACE_JS = "(a) => placeStone(a[0], a[1])"

    def __init__(self, page, args):
        self.page = page
        self.args = args

    def stones(self):
        b = self.page.evaluate("() => " + self.BOARD_JS)
        return {(c, r): ("B" if v == 1 else "W") for r, row in enumerate(b) for c, v in enumerate(row) if v}

    def result(self):
        if not self.page.evaluate("() => !!(" + self.OVER_JS + ")"):
            return None
        w = self.page.evaluate("() => " + self.WINNER_JS)
        return {1: "black", 2: "white"}.get(w, "draw")

    def place(self, x, y):
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            self.page.evaluate(self.PLACE_JS, [y, x])
            for _ in range(10):
                if (x, y) in self.stones():
                    return
                self.page.wait_for_timeout(30)
            # 相手 (サイト AI) が考え中などで拒否された場合は少し待って再試行
            self.page.wait_for_timeout(300)
        raise RuntimeError("move %d,%d was not accepted by the site" % (x, y))

    def wait_move(self, moves, timeout):
        known = set(moves)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            st = self.stones()
            new = [k for k in st if k not in known]
            if new:
                return new[0]
            if self.result():
                return None
            self.page.wait_for_timeout(100)
        return None
