"""gomocalc.com (Gomoku Calculator、Rapfi の Web 版) のアダプタ。

Vue アプリの Vuex ストアを直接読み書きする (document.querySelector('#app').__vue__.$root.$store)。
  state.position.position = 着手列 [[x, y], ...] (y は上から 0、座標表示は a..o / 15..1)
  state.position.winline  = 勝ち筋 (空でなければ終局)
  settings: rule (0 freestyle / 1 standard / 2 renju)、aiThinkBlack/aiThinkWhite (AI の手番)、
            thinkTimeOption=0 のとき turnTime/matchTime/maxDepth/maxNodes、threads (最大はブラウザ依存)
エンジンは Rapfi + NNUE (既定 Mix9svq)。AI の手番では盤面が変わると自動で思考して打つ。
LEVEL は 1 手の持ち時間 (ms)。例: --level 5000。
"""

import time

URL = "https://www.gomocalc.com/"
DEFAULT_LEVEL = "5000"
STORE = "document.querySelector('#app').__vue__.$root.$store"
RULES = {"freestyle": 0, "standard": 1, "renju": 2}
# 盤のコンポーネント (startThink() を持つ Vue インスタンス)。人間のクリック時だけ AI の思考が起動されるので、
# ストアで手を打った後にこれを呼んで AI の手番を始める
VM = """(() => {
  const find = (vm) => { if (vm.$options.methods && vm.$options.methods.startThink) return vm;
    for (const c of vm.$children) { const r = find(c); if (r) return r; } return null; };
  return find(document.querySelector('#app').__vue__.$root);
})()"""


class Site:
    def __init__(self, page, args):
        self.page = page
        self.args = args
        self.threads = int(getattr(args, "site_threads", 2) or 2)
        page.goto(URL, wait_until="networkidle", timeout=90000)

    def _set(self, key, value):
        self.page.evaluate("([k, v]) => %s.commit('settings/setValueNoSave', {key: k, value: v})" % STORE, [key, value])

    def new_game(self, rule, level, engine_color):
        p = self.page
        p.goto(URL, wait_until="networkidle", timeout=90000)
        p.wait_for_function("() => %s.state.ai.ready" % STORE, timeout=180000)
        for k, v in (("thinkTimeOption", 0), ("turnTime", int(level)), ("matchTime", 99999000), ("maxDepth", 64),
                     ("maxNodes", 0), ("threads", self.threads), ("rule", RULES[rule]), ("boardSize", 15),
                     ("aiThinkBlack", engine_color == "white"), ("aiThinkWhite", engine_color == "black")):
            self._set(k, v)
        p.evaluate("() => %s.commit('position/new', 15)" % STORE)
        p.evaluate("() => %s.dispatch('ai/restart')" % STORE)
        self.engine_color = engine_color
        self.rule = rule
        p.wait_for_timeout(500)
        self._kick()

    def _position(self):
        return self.page.evaluate("() => %s.state.position.position" % STORE)

    def stones(self):
        return {(x, y): "B" if i % 2 == 0 else "W" for i, (x, y) in enumerate(self._position())}

    def result(self):
        s = self.page.evaluate("() => ({w: %s.state.position.winline.length, n: %s.state.position.position.length})" % (STORE, STORE))
        if s["w"] > 0:
            return "black" if s["n"] % 2 == 1 else "white"
        if s["n"] >= 225:
            return "draw"
        return None

    def place(self, x, y):
        ok = self.page.evaluate("([x, y]) => %s.dispatch('position/makeMove', [x, y])" % STORE, [x, y])
        if not ok:
            raise RuntimeError("move %d,%d rejected" % (x, y))
        self._kick()

    def _kick(self):
        self.page.evaluate("() => { const vm = %s; if (vm.isAITurn && !vm.gameEnded && !vm.thinking) vm.startThink(); }" % VM)

    def wait_move(self, moves, timeout):
        n = len(moves)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            pos = self._position()
            if len(pos) > n:
                return tuple(pos[n])
            if self.result():
                return None
            self.page.wait_for_timeout(100)
        return None
