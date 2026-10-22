"""playgonu.com/gomoku のアダプタ。

盤は SVG。石は <circle fill=...> (黒 #242019 / 白 #F8F4E9)、手を打つ当たり判定は
class="board-point" の透明 circle (index = row*15+col の順)。難易度 Easy..Pro は JS の minimax、
Champion は Rapfi (WASM、1 手 5 秒) をブラウザ内で動かす。Champion は人間が黒 (先手) 固定。
"""

import time

URL = "https://playgonu.com/gomoku"
DEFAULT_LEVEL = "champion"
LEVELS = ["easy", "normal", "hard", "master", "pro", "champion"]
RULES = {"freestyle": "freestyle", "renju": "renju", "standard": "renju"}

_STONES_JS = """() => {
  const out = [];
  for (const c of document.querySelectorAll('[data-board-svg] circle')) {
    const f = c.getAttribute('fill');
    if (f !== '#242019' && f !== '#F8F4E9') continue;
    if (c.getAttribute('opacity')) continue;               // 着手プレビュー
    const r = parseFloat(c.getAttribute('r'));
    if (Math.abs(r - 28 * 0.38) > 0.2) continue;
    const col = Math.round((parseFloat(c.getAttribute('cx')) - 14) / 28);
    const row = Math.round((parseFloat(c.getAttribute('cy')) - 14) / 28);
    out.push([col, row, f === '#242019' ? 'B' : 'W']);
  }
  return out;
}"""


class Site:
    def __init__(self, page, args):
        self.page = page
        self.args = args
        page.goto(URL, wait_until="networkidle", timeout=90000)

    def new_game(self, rule, level, engine_color):
        p = self.page
        if level == "champion" and engine_color != "black":
            raise RuntimeError("playgonu champion plays White only (engine must be black)")
        # 対局中の確認ダイアログを避けるためページを読み直す
        p.goto(URL, wait_until="networkidle", timeout=90000)
        p.select_option("[data-ruleset]", RULES[rule])
        p.select_option("[data-side-select]", "A" if engine_color == "black" else "B")
        if level == "champion":
            p.select_option("[data-ai-difficulty]", "champion")
            # 紹介ダイアログ: エンジン (WASM) の読み込み完了を待って Play
            p.wait_for_selector(".win-banner.is-visible", timeout=30000)
            p.wait_for_function(
                "() => [...document.querySelectorAll('.win-banner.is-visible div')].some(d => /Ready to play|could not load/.test(d.textContent))",
                timeout=120000)
            txt = p.inner_text(".win-banner.is-visible")
            if "could not load" in txt:
                raise RuntimeError("champion engine failed to load")
            p.click(".win-banner.is-visible button.btn-primary")
        else:
            p.select_option("[data-ai-difficulty]", level)
            # 設定変更ごとに新規対局が走り、サイト AI (黒) の予約済みの着手が後から発火して二重に打つので、
            # 予約が消えるのを待ってから新規対局する
            p.wait_for_timeout(900)
            p.click("[data-new-game]")
        p.wait_for_timeout(300)
        # 盤をクリックで打つには sideSelect=A(自分が黒)。エンジンが白ならサイト AI が先に打つ
        self.engine_color = engine_color

    def stones(self):
        return {(c, r): col for c, r, col in self.page.evaluate(_STONES_JS)}

    def result(self):
        """None / 'black' / 'white' / 'draw' (終局表示を見る)。"""
        t = self.page.inner_text("[data-turn-label]")
        if "draw" in t.lower():
            return "draw"
        if "wins" in t:
            return "black" if t.startswith("Black") else "white"
        return None

    def place(self, x, y):
        idx = y * 15 + x
        self.page.evaluate("(i) => document.querySelectorAll('.board-point')[i].dispatchEvent(new MouseEvent('click', {bubbles: true}))", idx)
        # 反映待ち (石が増えるまで)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if (x, y) in self.stones():
                return
            self.page.wait_for_timeout(50)
        raise RuntimeError("move %d,%d was not accepted by the site" % (x, y))

    def wait_move(self, moves, timeout):
        """相手 (サイト AI) の次の手を待って (x, y) を返す。終局したら None。"""
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
