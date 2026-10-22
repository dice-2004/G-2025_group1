"""Gomocup (piskvork) プロトコルのエンジンをサブプロセスとして動かすラッパー。"""

import os
import queue
import shlex
import subprocess
import threading
import time


class EngineError(Exception):
    pass


class Engine:
    def __init__(self, cmd, cwd=None, name=None, log=None, pre_delay=0.0):
        # pre_delay: 着手要求の前に置く待ち時間 (秒)。Rapfi は手を返した直後に
        # 入力監視スレッドが次の行を食うことがあり、即座に TURN を送ると
        # "ERROR Unknown command: x,y" になるため、少し待ってから送る。
        self.pre_delay = pre_delay
        self.cmd = cmd if isinstance(cmd, list) else shlex.split(cmd)
        self.name = name or os.path.basename(self.cmd[0])
        self.log = log
        self.proc = subprocess.Popen(
            self.cmd, cwd=cwd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL, text=True, bufsize=1)
        self.lines = queue.Queue()
        self.reader = threading.Thread(target=self._read, daemon=True)
        self.reader.start()

    def _read(self):
        for line in self.proc.stdout:
            self.lines.put(line.rstrip("\r\n"))
        self.lines.put(None)

    def send(self, text):
        if self.log is not None:
            self.log.append(">%s %s" % (self.name, text))
        try:
            self.proc.stdin.write(text + "\n")
            self.proc.stdin.flush()
        except (BrokenPipeError, OSError) as e:
            raise EngineError("%s: pipe closed (%s)" % (self.name, e))

    def _readline(self, deadline):
        while True:
            left = deadline - time.monotonic()
            if left <= 0:
                raise EngineError("%s: timeout" % self.name)
            try:
                line = self.lines.get(timeout=left)
            except queue.Empty:
                raise EngineError("%s: timeout" % self.name)
            if line is None:
                raise EngineError("%s: process exited" % self.name)
            if self.log is not None:
                self.log.append("<%s %s" % (self.name, line))
            u = line.strip().upper()
            if not u or u.startswith(("MESSAGE", "DEBUG", "UNKNOWN")):
                continue
            return line.strip()

    def start(self, size=15, timeout_turn=1000, rule=0, extra_info=None, wait=30.0):
        self.send("START %d" % size)
        resp = self._readline(time.monotonic() + wait)
        if not resp.upper().startswith("OK"):
            raise EngineError("%s: START failed: %s" % (self.name, resp))
        self.send("INFO timeout_turn %d" % timeout_turn)
        self.send("INFO timeout_match 100000000")
        self.send("INFO time_left 100000000")
        self.send("INFO max_memory 536870912")
        self.send("INFO game_type 1")
        self.send("INFO rule %d" % rule)
        for k, v in (extra_info or {}).items():
            self.send("INFO %s %s" % (k, v))

    def _read_move(self, deadline, t0):
        resp = self._readline(deadline)
        try:
            x, y = (int(v) for v in resp.split(","))
        except ValueError:
            raise EngineError("%s: bad move response: %r" % (self.name, resp))
        return x, y, time.monotonic() - t0

    def begin(self, deadline):
        if self.pre_delay:
            time.sleep(self.pre_delay)
        t0 = time.monotonic()
        self.send("BEGIN")
        return self._read_move(deadline, t0)

    def turn(self, x, y, deadline):
        if self.pre_delay:
            time.sleep(self.pre_delay)
        t0 = time.monotonic()
        self.send("TURN %d,%d" % (x, y))
        return self._read_move(deadline, t0)

    def board(self, stones, deadline):
        """stones: [(x, y, color)] color は 1=手番側(自分) 2=相手。"""
        if self.pre_delay:
            time.sleep(self.pre_delay)
        t0 = time.monotonic()
        self.send("BOARD")
        for x, y, c in stones:
            self.send("%d,%d,%d" % (x, y, c))
        self.send("DONE")
        return self._read_move(deadline, t0)

    def close(self):
        try:
            self.send("END")
        except EngineError:
            pass
        try:
            self.proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait()
