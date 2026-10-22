"""五目並べのルール判定 (ハーネス用レフェリー)。

対応ルール:
  freestyle : 五連以上で勝ち (禁じ手なし)                  Gomocup rule=0
  standard  : ちょうど五連で勝ち、長連は勝ちにならない        Gomocup rule=1
  renju     : 連珠。黒のみ長連/四四/三三が禁じ手(打つと負け)  Gomocup rule=4
  class     : 授業の審判 (.libgomoku.c の gameEndProcess) をそのまま使う。
              check 変数 (1..7) で有効な禁じ手が変わる。審判の判定は
              本物の連珠と違う癖があるので、本番用にはこれで検証すること。

座標は board[y][x]、石は 0=空 1=黒 2=白 (授業の枠組みと同じ)。
"""

import ctypes
import os
import subprocess
import sys

N = 15
EMPTY, BLACK, WHITE = 0, 1, 2
DIRS = ((1, 0), (0, 1), (1, 1), (1, -1))

RULE_GOMOCUP_CODE = {"freestyle": 0, "standard": 1, "renju": 4, "class": 4}


def inside(x, y):
    return 0 <= x < N and 0 <= y < N


def run_length(board, x, y, dx, dy, color):
    """(x,y) を含む方向 (dx,dy) の連続石数。(x,y) 自身は color とみなす。"""
    n = 1
    cx, cy = x + dx, y + dy
    while inside(cx, cy) and board[cy][cx] == color:
        n += 1
        cx += dx
        cy += dy
    cx, cy = x - dx, y - dy
    while inside(cx, cy) and board[cy][cx] == color:
        n += 1
        cx -= dx
        cy -= dy
    return n


def max_run(board, x, y, color):
    return max(run_length(board, x, y, dx, dy, color) for dx, dy in DIRS)


# ---------------------------------------------------------------- renju
def _five_points(board, x, y, dx, dy):
    """(x,y) の黒石を含んでちょうど五連になる、その方向の空点リスト (黒)。"""
    pts = []
    for k in range(-4, 5):
        if k == 0:
            continue
        qx, qy = x + k * dx, y + k * dy
        if not inside(qx, qy) or board[qy][qx] != EMPTY:
            continue
        board[qy][qx] = BLACK
        if run_length(board, x, y, dx, dy, BLACK) == 5:
            pts.append(k)
        board[qy][qx] = EMPTY
    return pts


def _four_count_dir(board, x, y, dx, dy):
    pts = _five_points(board, x, y, dx, dy)
    if not pts:
        return 0
    if len(pts) == 2 and pts[1] - pts[0] == 5:
        return 1  # 達四 (_XXXX_) は四一つ
    return min(len(pts), 2)


def _is_straight_four_dir(board, x, y, dx, dy):
    pts = _five_points(board, x, y, dx, dy)
    return len(pts) == 2 and pts[1] - pts[0] == 5


def _is_three_dir(board, x, y, dx, dy, depth):
    """方向 (dx,dy) で (x,y) を含む「活三」か (禁じ手でない手で達四にできるか)。"""
    for k in range(-4, 5):
        if k == 0:
            continue
        qx, qy = x + k * dx, y + k * dy
        if not inside(qx, qy) or board[qy][qx] != EMPTY:
            continue
        board[qy][qx] = BLACK
        ok = False
        if _is_straight_four_dir(board, x, y, dx, dy):
            ok = not _forbidden_placed(board, qx, qy, depth + 1)
        board[qy][qx] = EMPTY
        if ok:
            return True
    return False


def _forbidden_placed(board, x, y, depth=0):
    """黒石が (x,y) に既に置かれている前提で禁じ手か判定。"""
    runs = [run_length(board, x, y, dx, dy, BLACK) for dx, dy in DIRS]
    if 5 in runs:
        return False  # 五連は禁じ手より優先
    if any(r >= 6 for r in runs):
        return True
    if depth > 6:
        return False
    fours = 0
    four_dirs = set()
    for i, (dx, dy) in enumerate(DIRS):
        c = _four_count_dir(board, x, y, dx, dy)
        if c:
            fours += c
            four_dirs.add(i)
    if fours >= 2:
        return True
    threes = 0
    for i, (dx, dy) in enumerate(DIRS):
        if i in four_dirs:
            continue
        if _is_three_dir(board, x, y, dx, dy, depth):
            threes += 1
            if threes >= 2:
                return True
    return False


def renju_forbidden_reason(board, x, y):
    """黒が (x,y) に打つと禁じ手なら理由 ('overline'/'44'/'33')、そうでなければ None。"""
    if board[y][x] != EMPTY:
        return None
    board[y][x] = BLACK
    try:
        runs = [run_length(board, x, y, dx, dy, BLACK) for dx, dy in DIRS]
        if 5 in runs:
            return None
        if any(r >= 6 for r in runs):
            return "overline"
        fours = sum(_four_count_dir(board, x, y, dx, dy) for dx, dy in DIRS)
        if fours >= 2:
            return "44"
        if _forbidden_placed(board, x, y):
            return "33"
        return None
    finally:
        board[y][x] = EMPTY


# ---------------------------------------------------------------- class judge
_HERE = os.path.dirname(os.path.abspath(__file__))
_REPO = os.path.dirname(_HERE)
_LIB_PATH = os.path.join(_HERE, "build", "libclassjudge.so")
_lib = None


def _load_class_lib():
    global _lib
    if _lib is not None:
        return _lib
    src = [os.path.join(_REPO, ".libgomoku.c"), os.path.join(_HERE, "judge_stub.c")]
    if not os.path.exists(_LIB_PATH) or any(
        os.path.getmtime(s) > os.path.getmtime(_LIB_PATH) for s in src
    ):
        os.makedirs(os.path.dirname(_LIB_PATH), exist_ok=True)
        # printf を握りつぶす: 審判は判定のたびに大量に出力するため
        cmd = ["gcc", "-O1", "-U_FORTIFY_SOURCE", "-D_FORTIFY_SOURCE=0", "-shared", "-fPIC",
               "-Dprintf=judge_quiet_printf", "-I", _REPO, "-o", _LIB_PATH] + src
        subprocess.check_call(cmd)
    lib = ctypes.CDLL(_LIB_PATH)
    lib.gameEndProcess.restype = ctypes.c_int
    lib.judge_set_check.argtypes = [ctypes.c_int]
    _lib = lib
    return lib


Row = ctypes.c_int * N
Grid = Row * N


class ClassJudge:
    """授業審判の状態 (c33Board/c44Board) を 1 局ぶん保持するラッパー。"""

    def __init__(self, check=7):
        self.lib = _load_class_lib()
        self.check = check
        self.c33 = Grid()
        self.c44 = Grid()

    def after_move(self, board):
        """着手後の盤面で審判を走らせる。勝負がついたら True。"""
        self.lib.judge_set_check(self.check)
        g = Grid()
        for y in range(N):
            for x in range(N):
                g[y][x] = board[y][x]
        return bool(self.lib.gameEndProcess(g, self.c33, self.c44))


# ---------------------------------------------------------------- referee
class Referee:
    """1 局分の勝敗判定。play() は (終局?, 勝者, 理由) を返す。"""

    def __init__(self, rule="freestyle", check=7):
        if rule not in RULE_GOMOCUP_CODE:
            raise ValueError("unknown rule: %s" % rule)
        self.rule = rule
        self.check = check
        self.board = [[EMPTY] * N for _ in range(N)]
        self.moves = 0
        self.judge = ClassJudge(check) if rule == "class" else None

    def to_move(self):
        return BLACK if self.moves % 2 == 0 else WHITE

    def play(self, x, y):
        color = self.to_move()
        b = self.board
        if not inside(x, y) or b[y][x] != EMPTY:
            return True, 3 - color, "illegal"
        other = 3 - color
        if self.rule == "renju" and color == BLACK:
            reason = renju_forbidden_reason(b, x, y)
            if reason:
                b[y][x] = color
                self.moves += 1
                return True, WHITE, "forbidden-" + reason
        b[y][x] = color
        self.moves += 1
        if self.rule == "class":
            if self.judge.after_move(b):
                return True, self._class_winner(), self._class_reason()
        else:
            r = max_run(b, x, y, color)
            if self.rule == "freestyle" and r >= 5:
                return True, color, "five"
            if self.rule == "standard" and r == 5:
                return True, color, "five"
            if self.rule == "renju" and (r == 5 or (color == WHITE and r >= 5)):
                return True, color, "five"
        if self.moves >= N * N:
            return True, EMPTY, "full"
        return False, None, None

    # 授業審判の終局理由を再構成する (gameEndProcess と同じ優先順位)
    def _class_lines(self):
        b = self.board
        for y in range(N):
            for x in range(N):
                if b[y][x] == EMPTY:
                    continue
                for dx, dy in ((1, 1), (0, 1), (1, 0), (-1, 1)):
                    yield x, y, dx, dy

    def _count_from(self, x, y, dx, dy, k):
        c = self.board[y][x]
        for j in range(k):
            cx, cy = x + j * dx, y + j * dy
            if not inside(cx, cy) or self.board[cy][cx] != c:
                return False
        return True

    def _class_winner(self):
        if self.check & 4:
            for x, y, dx, dy in self._class_lines():
                if self.board[y][x] == BLACK and self._count_from(x, y, dx, dy, 6):
                    return WHITE
        for x, y, dx, dy in self._class_lines():
            if self._count_from(x, y, dx, dy, 5):
                return self.board[y][x]
        return WHITE

    def _class_reason(self):
        if self.check & 4:
            for x, y, dx, dy in self._class_lines():
                if self.board[y][x] == BLACK and self._count_from(x, y, dx, dy, 6):
                    return "forbidden-overline"
        for x, y, dx, dy in self._class_lines():
            if self._count_from(x, y, dx, dy, 5):
                return "five"
        return "forbidden-33/44(class)"


if __name__ == "__main__":
    # 簡単な自己テスト
    b = [[EMPTY] * N for _ in range(N)]
    for x, y in ((7, 7), (8, 7), (7, 8)):
        b[y][x] = BLACK
    b[6][7] = EMPTY
    # (7,7)(8,7) 横 + (7,8) 縦 に (9,7)? -> 横三のみ
    assert renju_forbidden_reason(b, 6, 7) is None
    # 三三: 横 (6,7)(7,7)(8,7) と 縦 (6,5)(6,6)(6,7)
    b = [[EMPTY] * N for _ in range(N)]
    for x, y in ((7, 7), (8, 7), (6, 5), (6, 6)):
        b[y][x] = BLACK
    assert renju_forbidden_reason(b, 6, 7) == "33", renju_forbidden_reason(b, 6, 7)
    # 四四: 横 (4..6,7) と 縦 (7,4..6) -> (7,7)
    b = [[EMPTY] * N for _ in range(N)]
    for x, y in ((4, 7), (5, 7), (6, 7), (7, 4), (7, 5), (7, 6)):
        b[y][x] = BLACK
    assert renju_forbidden_reason(b, 7, 7) == "44"
    # 長連
    b = [[EMPTY] * N for _ in range(N)]
    for x in (2, 3, 4, 6, 7):
        b[7][x] = BLACK
    assert renju_forbidden_reason(b, 5, 7) == "overline"
    # 片側止めの三は活三でない -> 三三にならない
    b = [[EMPTY] * N for _ in range(N)]
    for x, y in ((7, 7), (8, 7), (6, 5), (6, 6)):
        b[y][x] = BLACK
    b[7][9] = WHITE
    b[7][5] = WHITE
    assert renju_forbidden_reason(b, 6, 7) is None
    # 一直線上の四四 (X.XXX.X の中央)
    b = [[EMPTY] * N for _ in range(N)]
    for x in (3, 5, 7, 9):
        b[7][x] = BLACK
    assert renju_forbidden_reason(b, 6, 7) == "44", renju_forbidden_reason(b, 6, 7)
    # 授業審判: ロードと五連判定
    r = Referee("class", 7)
    seq = [(7, 7), (0, 0), (8, 7), (0, 1), (9, 7), (0, 3), (10, 7), (0, 5), (11, 7)]
    res = None
    for x, y in seq:
        res = r.play(x, y)
    assert res == (True, BLACK, "five"), res
    print("rules self-test OK")
