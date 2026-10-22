"""trygomoku.com のアダプタ。ルール freestyle/renju、盤 9/15/19、難易度 easy/medium/hard/master。

ゲームの状態は game.js のトップレベル変数 (board, gameOver, winner)。打つのは placeStone(row, col)
(盤の click ハンドラが呼ぶ関数そのもの)。Master は反復深化 2→12 手読み (最大 12 秒) + VCF/脅威検出の JS AI。
"""

from .jsstate import JsStateSite

URL = "https://trygomoku.com/"
DEFAULT_LEVEL = "master"
LEVELS = ["easy", "medium", "hard", "master"]


class Site(JsStateSite):
    def __init__(self, page, args):
        super().__init__(page, args)
        page.goto(URL, wait_until="networkidle", timeout=90000)

    def new_game(self, rule, level, engine_color):
        p = self.page
        if rule == "standard":
            raise RuntimeError("trygomoku has no standard rule")
        p.goto(URL, wait_until="networkidle", timeout=90000)
        p.evaluate("""([rule, level, color]) => {
            switchDifficulty(level);
            if (gameRule !== rule) switchRule(rule);
            // 人間役 (= こちらのエンジン) の色。AI が黒ならサイト AI が先に打つ
            switchColor(color === 'black' ? BLACK : WHITE);
        }""", [rule, level, engine_color])
        p.wait_for_timeout(300)
        ok = p.evaluate("() => [gameRule, difficulty, humanColor, boardSize]")
        assert ok[0] == rule and ok[1] == level and ok[3] == 15, ok
