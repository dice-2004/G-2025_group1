"""gomokuonline.org のアダプタ。freestyle (五連以上で勝ち) のみ、盤 15。難易度 easy/medium/hard/expert。

window.app (GomokuApp) の game (盤 [row][col] 0/1/2, gameOver, winner) を読み、
app.handleCellClick(row, col) で打つ。AI は常に白なので、エンジンは黒番のみ。
Expert は advanced-ai.js (深さ 10・約 3 秒)。
"""

import time

from .jsstate import JsStateSite

URL = "https://gomokuonline.org/"
DEFAULT_LEVEL = "expert"
LEVELS = ["easy", "medium", "hard", "expert"]


class Site(JsStateSite):
    BOARD_JS = "window.app.game.board"
    OVER_JS = "window.app.game.gameOver"
    WINNER_JS = "window.app.game.winner"
    PLACE_JS = "(a) => window.app.handleCellClick(a[0], a[1])"

    def __init__(self, page, args):
        super().__init__(page, args)
        page.goto(URL, wait_until="networkidle", timeout=90000)

    def new_game(self, rule, level, engine_color):
        if rule != "freestyle":
            raise RuntimeError("gomokuonline.org supports freestyle only")
        if engine_color != "black":
            raise RuntimeError("gomokuonline.org AI plays White only (engine must be black)")
        p = self.page
        p.goto(URL, wait_until="networkidle", timeout=90000)
        p.select_option("#difficultySelect", level)
        p.click("#newGameBtn")
        p.wait_for_timeout(300)
        assert p.evaluate("() => window.app.ai.difficulty") == level
