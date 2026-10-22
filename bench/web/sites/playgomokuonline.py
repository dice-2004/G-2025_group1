"""playgomokuonline.com/play のアダプタ。盤は canvas、状態は右の「MOVE HISTORY」の文字列 (G8 (Black) ...)。

設定: AI Strength スライダー 1..10 (Vue の input)、Forbidden Rules (Renju) チェックボックス (黒の禁じ手)、
Your Color。AI は Web Worker 内で動く。着手は canvas の座標クリック (列 A..O = x、行 1..15 = y+1、上が 1)。
"""

import re
import time

URL = "https://playgomokuonline.com/play"
DEFAULT_LEVEL = "10"
LEVELS = [str(i) for i in range(1, 11)]
_MOVE = re.compile(r"\b([A-O])(\d{1,2})\s*\((Black|White)\)")


class Site:
    def __init__(self, page, args):
        self.page = page
        self.args = args
        page.goto(URL, wait_until="networkidle", timeout=90000)

    def new_game(self, rule, level, engine_color):
        p = self.page
        if rule == "standard":
            raise RuntimeError("no standard rule")
        p.goto(URL, wait_until="networkidle", timeout=90000)
        p.evaluate("""([level]) => {
            const r = document.querySelector('input[type=range]');
            const set = Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value').set;
            set.call(r, level); r.dispatchEvent(new Event('input', {bubbles: true}));
        }""", [level])
        cb = p.locator("#forbidden-rules")
        if cb.is_checked() != (rule == "renju"):
            cb.click()
        # Your Color は最後の select (value black/white)。サイト AI の色の反対 = エンジンの色
        p.locator("select").nth(3).select_option(engine_color)
        p.wait_for_timeout(300)
        assert p.locator("input[type=range]").input_value() == str(level)
        self.engine_color = engine_color

    def _history(self):
        txt = self.page.inner_text("body")
        out = []
        for m in _MOVE.finditer(txt):
            out.append((ord(m.group(1)) - 65, int(m.group(2)) - 1))
        return out

    def stones(self):
        return {m: "B" if i % 2 == 0 else "W" for i, m in enumerate(self._history())}

    def result(self):
        txt = self.page.inner_text("body")
        m = re.search(r"(Black|White) wins|(Black|White) Wins|You win|You Win|AI wins|Draw", txt)
        if not m:
            return None
        s = m.group(0).lower()
        if "draw" in s:
            return "draw"
        if s.startswith("black"):
            return "black"
        if s.startswith("white"):
            return "white"
        # "You win" / "AI wins"
        mine = self.engine_color
        theirs = "white" if mine == "black" else "black"
        return mine if s.startswith("you") else theirs

    def place(self, x, y):
        box = self.page.locator("canvas").bounding_box()
        margin = 32 / 470 * box["width"]
        step = (box["width"] - 2 * margin) / 14
        self.page.mouse.click(box["x"] + margin + x * step, box["y"] + margin + y * step)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if (x, y) in self.stones():
                return
            self.page.wait_for_timeout(50)
        raise RuntimeError("move %d,%d was not accepted by the site" % (x, y))

    def wait_move(self, moves, timeout):
        n = len(moves)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            h = self._history()
            if len(h) > n:
                return h[n]
            if self.result():
                return None
            self.page.wait_for_timeout(100)
        return None
