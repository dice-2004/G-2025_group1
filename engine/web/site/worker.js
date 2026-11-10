// 思考用の Worker。盤の状態は wasm の中に持ち、UI からの命令を順に処理する
const V = self.location.search;   // ?v=<ビルド> (キャッシュよけ)
if (typeof G2Wasi === 'undefined') importScripts('wasi.js' + V);   // 1 ファイル版では前に連結済み

let g = null, ready = null;
function boot(bytes) {
  ready = (async () => { g = await G2Wasi.loadG2(await bytes, () => {}); })();
}

function cstr(ptr) {
  const mem = new Uint8Array(g.memory.buffer);
  let end = ptr;
  while (mem[end]) end++;
  return new TextDecoder().decode(mem.subarray(ptr, end));
}

function state(size) {
  // 手番が黒のときの禁じ手 (連珠のみ) と、勝負の結果
  const forbidden = [];
  for (let y = 0; y < size; y++) for (let x = 0; x < size; x++) if (g.g2_forbidden(x, y)) forbidden.push([x, y]);
  return { forbidden, result: g.g2_result() };
}

self.onmessage = async (ev) => {
  const m = ev.data;
  if (m.type === 'init') { boot(m.bytes); return; }   // 1 ファイル版: wasm の中身を受け取る
  if (!ready) boot(fetch('g2.wasm' + V).then((r) => r.arrayBuffer()));
  try {
    await ready;
    if (m.type === 'new') {
      g.g2_new_game(m.rule, m.size);
      for (const [x, y] of m.moves || []) g.g2_play(x, y);
      self.postMessage({ type: 'state', id: m.id, version: cstr(g.g2_version()), ...state(m.size) });
    } else if (m.type === 'play') {
      const ok = g.g2_play(m.x, m.y);
      self.postMessage({ type: 'state', id: m.id, ok: !!ok, ...state(m.size) });
    } else if (m.type === 'undo') {
      for (let i = 0; i < m.n; i++) g.g2_undo();
      self.postMessage({ type: 'state', id: m.id, ...state(m.size) });
    } else if (m.type === 'think') {
      const t0 = performance.now();
      const r = g.g2_think(m.ms);
      const wall = Math.round(performance.now() - t0);
      const pv = [];
      for (let i = 0, p; (p = g.g2_pv(i)) >= 0; i++) pv.push([p & 31, p >> 5]);
      const info = {
        score: g.g2_info(0), depth: g.g2_info(1), ms: g.g2_info(3), book: !!g.g2_info(4),
        seldepth: g.g2_info(5), nodes: g.g2_info(7), reason: cstr(g.g2_reason()), pv, wall,
      };
      self.postMessage({ type: 'move', id: m.id, x: r < 0 ? -1 : r & 31, y: r < 0 ? -1 : r >> 5, info });
    }
  } catch (e) {
    self.postMessage({ type: 'error', id: m.id, message: String(e && e.message || e) });
  }
};
