// 盤面 UI。思考は worker.js (G2 の wasm) に任せる
(() => {
  const SIZE = 15;
  const COLS = 'ABCDEFGHJKLMNOPQRST';   // 慣例に合わせて I を飛ばす
  const WIN_MIN = 29000;                // search.h の SCORE_WIN_MIN

  const $ = (id) => document.getElementById(id);
  // CI が index.html の __BUILD__ をコミットと日付に置き換える。ファイルの URL に付けて古いキャッシュを避ける
  const BUILD = (document.querySelector('meta[name="g2-build"]') || {}).content || 'dev';
  $('build').textContent = BUILD === '__BUILD__' ? 'dev' : BUILD;
  const canvas = $('board'), ctx = canvas.getContext('2d');
  const statusEl = $('status'), infoEl = $('info'), movesEl = $('moves');
  const statsEl = $('stats'), logEl = $('log'), engineEl = $('engine');
  const REASONS = {
    search: '探索 (αβ)', vcf: '詰み探索 (VCF: 四の連続)', vct: '詰み探索 (VCT: 三と四の連続)', ts: '脅威探索',
    five: '五を打つ', 'block-five': '相手の五を止める', flex4: '活四を作る', center: '初手 (天元)',
    'only-move': '受けがこの 1 手だけ', verified: '詰みの受けを確かめた手', lost: '負けを読み切り', book: '開局ブック',
  };
  const fmtInt = (n) => n.toLocaleString('ja-JP');

  let worker = null;
  let moves = [];            // [[x, y], ...] 黒から交互
  let rule = 4, human = 1;   // human: 1=黒 2=白
  let forbidden = [];
  let result = 0;            // 0=続行 1=黒勝ち 2=白勝ち 3=引き分け
  let busy = false;          // AI の思考中
  let game = 0;              // 古い応答を捨てるための対局番号
  let hover = null;
  let nextId = 1;
  const pending = new Map();
  // 棋譜の記録 (docs: engine/web/README.md の「対局データ」)
  let thoughts = [];         // moves と同じ長さ。G2 の手なら思考の情報、あなたの手なら null
  let engineVersion = '';
  let rec = null;            // { id, started_at, undos, saved_key, indexed }
  const RULE_NAMES = { 0: 'freestyle', 1: 'standard', 4: 'renju' };

  function call(msg) {
    return new Promise((resolve, reject) => {
      const id = nextId++;
      pending.set(id, { resolve, reject });
      worker.postMessage({ ...msg, id, size: SIZE });
    });
  }

  function startWorker() {
    const inline = window.G2_INLINE;   // 1 ファイル版 (make_artifact.py) では Worker と wasm が埋め込まれている
    if (inline) {
      worker = new Worker(URL.createObjectURL(new Blob([inline.worker], { type: 'text/javascript' })));
      const bytes = Uint8Array.from(atob(inline.wasm), (c) => c.charCodeAt(0));
      worker.postMessage({ type: 'init', bytes: bytes.buffer }, [bytes.buffer]);
    } else {
      worker = new Worker('worker.js?v=' + encodeURIComponent(BUILD));
    }
    worker.onmessage = (ev) => {
      const m = ev.data, p = pending.get(m.id);
      if (!p) return;
      pending.delete(m.id);
      if (m.type === 'error') p.reject(new Error(m.message)); else p.resolve(m);
    };
    worker.onerror = (e) => { setStatus('AI を読み込めませんでした: ' + (e.message || 'エラー'), 'lose'); };
  }

  const sideToMove = () => (moves.length % 2 === 0 ? 1 : 2);
  const name = (c) => (c === 1 ? '黒' : '白');
  const coord = ([x, y]) => COLS[x] + (SIZE - y);

  function setStatus(text, cls) {
    statusEl.textContent = text;
    statusEl.className = 'status' + (cls ? ' ' + cls : '');
  }

  function applyState(s) {
    forbidden = s.forbidden || [];
    result = s.result || 0;
  }

  function updateStatus() {
    if (result) {
      if (result === 3) setStatus('引き分けです', '');
      else if (result === human) setStatus('あなたの勝ちです！', 'win');
      else setStatus('G2 の勝ちです', 'lose');
    } else if (busy) {
      setStatus('G2 が考えています…', '');
    } else if (sideToMove() === human) {
      setStatus(`あなたの番です（${name(human)}）`, '');
    }
    $('undo').disabled = busy || moves.length === 0;
    canvas.classList.toggle('busy', busy);
    movesEl.textContent = moves.length ? moves.map((m, i) => `${i + 1}.${coord(m)}`).join('  ') : '—';
    movesEl.scrollTop = movesEl.scrollHeight;
  }

  // ---- 描画 ----
  function css(name) { return getComputedStyle(document.documentElement).getPropertyValue(name).trim(); }

  function geometry() {
    const W = canvas.width, margin = W * 0.065, cell = (W - 2 * margin) / (SIZE - 1);
    return { W, margin, cell };
  }

  function draw() {
    const { W, margin, cell } = geometry();
    ctx.fillStyle = css('--board');
    ctx.fillRect(0, 0, W, W);
    ctx.strokeStyle = css('--grid');
    ctx.lineWidth = Math.max(1, W / 720);
    for (let i = 0; i < SIZE; i++) {
      const p = margin + i * cell;
      ctx.beginPath(); ctx.moveTo(margin, p); ctx.lineTo(W - margin, p); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(p, margin); ctx.lineTo(p, W - margin); ctx.stroke();
    }
    ctx.fillStyle = css('--grid');
    for (const [x, y] of [[3, 3], [11, 3], [7, 7], [3, 11], [11, 11]]) {
      ctx.beginPath(); ctx.arc(margin + x * cell, margin + y * cell, cell * 0.1, 0, Math.PI * 2); ctx.fill();
    }
    ctx.font = `${Math.round(cell * 0.36)}px system-ui, sans-serif`;
    ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
    for (let i = 0; i < SIZE; i++) {
      ctx.fillText(COLS[i], margin + i * cell, margin * 0.42);
      ctx.fillText(String(SIZE - i), margin * 0.42, margin + i * cell);
    }
    // 禁じ手 (あなたが黒番のとき)
    if (!result && !busy && human === 1 && sideToMove() === 1) {
      ctx.strokeStyle = css('--danger');
      ctx.lineWidth = Math.max(2, cell * 0.08);
      const r = cell * 0.2;
      for (const [x, y] of forbidden) {
        const cx = margin + x * cell, cy = margin + y * cell;
        ctx.beginPath(); ctx.moveTo(cx - r, cy - r); ctx.lineTo(cx + r, cy + r);
        ctx.moveTo(cx + r, cy - r); ctx.lineTo(cx - r, cy + r); ctx.stroke();
      }
    }
    const showNum = $('numbers').value === '1';
    moves.forEach(([x, y], i) => {
      const cx = margin + x * cell, cy = margin + y * cell, black = i % 2 === 0;
      const grad = ctx.createRadialGradient(cx - cell * 0.15, cy - cell * 0.15, cell * 0.05, cx, cy, cell * 0.47);
      if (black) { grad.addColorStop(0, '#555'); grad.addColorStop(1, '#111'); }
      else { grad.addColorStop(0, '#fff'); grad.addColorStop(1, '#d6d3cc'); }
      ctx.fillStyle = grad;
      ctx.beginPath(); ctx.arc(cx, cy, cell * 0.46, 0, Math.PI * 2); ctx.fill();
      if (!black) { ctx.strokeStyle = 'rgba(0,0,0,.35)'; ctx.lineWidth = 1; ctx.stroke(); }
      const last = i === moves.length - 1;
      if (showNum) {
        ctx.fillStyle = last ? '#e0442a' : (black ? '#fff' : '#111');
        ctx.font = `${last ? 'bold ' : ''}${Math.round(cell * (i >= 99 ? 0.34 : 0.42))}px system-ui, sans-serif`;
        ctx.fillText(String(i + 1), cx, cy + 1);
      } else if (last) {
        ctx.fillStyle = '#e0442a';
        ctx.beginPath(); ctx.arc(cx, cy, cell * 0.12, 0, Math.PI * 2); ctx.fill();
      }
    });
    // 置く場所の目安
    if (hover && canPlay() && !occupied(hover[0], hover[1])) {
      const cx = margin + hover[0] * cell, cy = margin + hover[1] * cell;
      ctx.globalAlpha = 0.35;
      ctx.fillStyle = human === 1 ? '#111' : '#fff';
      ctx.beginPath(); ctx.arc(cx, cy, cell * 0.46, 0, Math.PI * 2); ctx.fill();
      ctx.globalAlpha = 1;
    }
  }

  function resize() {
    const px = Math.round(canvas.getBoundingClientRect().width * (window.devicePixelRatio || 1));
    if (px > 0 && Math.abs(px - canvas.width) > 2) { canvas.width = px; canvas.height = px; }
    draw();
  }

  function cellAt(ev) {
    const rect = canvas.getBoundingClientRect();
    const { margin, cell } = geometry();
    const sx = canvas.width / rect.width;
    const x = Math.round(((ev.clientX - rect.left) * sx - margin) / cell);
    const y = Math.round(((ev.clientY - rect.top) * sx - margin) / cell);
    return x >= 0 && y >= 0 && x < SIZE && y < SIZE ? [x, y] : null;
  }

  const occupied = (x, y) => moves.some(([a, b]) => a === x && b === y);
  const canPlay = () => worker && !busy && !result && sideToMove() === human;

  // ---- 対局の進行 ----
  async function play(x, y, info) {
    const g = game;
    const s = await call({ type: 'play', x, y });
    if (g !== game || !s.ok) return false;
    moves.push([x, y]);
    thoughts.push(info || null);
    applyState(s);
    if (result || (info && moves.length >= 6)) saveGame();   // 終局と、6 手目以降の G2 の手ごと (途中で閉じられても残る)
    return true;
  }

  async function aiMove() {
    if (result || sideToMove() === human) return;
    const g = game;
    busy = true; updateStatus(); draw();
    try {
      const r = await call({ type: 'think', ms: Number($('time').value) });
      if (g !== game) return;
      if (r.x < 0) { result = 3; return; }
      await play(r.x, r.y, { ...r.info, think_ms: Number($('time').value) });
      if (g !== game) return;
      showThought(r, moves.length);
    } catch (e) {
      setStatus('エラー: ' + e.message, 'lose');
    } finally {
      if (g === game) { busy = false; updateStatus(); draw(); }
    }
  }

  // 直前の思考を表に出し、記録に 1 行足す
  function showThought(r, ply) {
    const i = r.info, mv = coord([r.x, r.y]);
    const reason = i.book ? REASONS.book : (REASONS[i.reason] || i.reason);
    const sc = i.book ? '—' : i.score >= WIN_MIN ? `勝ちを読み切り (${i.score})` : i.score <= -WIN_MIN ? `負けを読み切り (${i.score})` : (i.score > 0 ? '+' : '') + i.score;
    const nps = i.ms > 0 ? Math.round(i.nodes / i.ms * 1000) : 0;
    const rows = [['手', `${ply}. ${mv}`], ['決め手', reason]];
    if (!i.book) {
      if (i.depth > 0) rows.push(['深さ', `${i.depth}（最大 ${i.seldepth}）`]);
      rows.push(['評価 (G2 から見て)', sc], ['ノード数', fmtInt(i.nodes)], ['思考時間', `${(i.wall / 1000).toFixed(2)} 秒`]);
      if (i.nodes > 0) rows.push(['速さ', `${fmtInt(nps)} ノード/秒`]);
      if (i.pv.length > 1) rows.push(['読み筋', i.pv.slice(0, 12).map(coord).join(' ')]);
    }
    statsEl.replaceChildren(...rows.flatMap(([k, v]) => {
      const dt = document.createElement('dt'), dd = document.createElement('dd');
      dt.textContent = k; dd.textContent = v;
      return [dt, dd];
    }));
    statsEl.hidden = false;
    infoEl.textContent = '';
    const li = document.createElement('li');
    li.textContent = i.book ? `${ply}. ${mv} 開局ブック`
      : `${ply}. ${mv} ${i.depth > 0 ? `深さ ${i.depth}/${i.seldepth} ` : ''}評価 ${sc} ${fmtInt(i.nodes)} ノード ${(i.wall / 1000).toFixed(2)} 秒${i.nodes > 0 ? ` ${fmtInt(nps)}/秒` : ''}`;
    if (logEl.dataset.empty !== '0') { logEl.replaceChildren(); logEl.dataset.empty = '0'; }
    logEl.append(li);
    logEl.scrollTop = logEl.scrollHeight;
  }

  function showEngine(version) {
    // wasm 自身が返す版 ("G2 <コミット>")。コミットなら GitHub へのリンクを付ける
    const hash = (version.match(/\b[0-9a-f]{7,40}\b/) || [])[0];
    engineEl.replaceChildren(`エンジン: ${version}（WebAssembly・1 スレッド・この端末で計算）`);
    if (hash) {
      const a = document.createElement('a');
      a.href = `https://github.com/dice-2004/G-2025_group1/tree/${hash}/engine/src`;
      a.target = '_blank'; a.rel = 'noopener';
      a.textContent = 'このコミットのソース';
      engineEl.append(' ', a);
    }
  }

  // ---- 棋譜 ----
  function gameRecord() {
    const res = result === 1 ? 'black' : result === 2 ? 'white' : result === 3 ? 'draw' : null;
    return {
      format: 'g2-game/1',
      id: rec.id,
      engine: engineVersion,
      page_build: BUILD,
      rule: RULE_NAMES[rule] || String(rule),
      gomocup_rule: rule,
      board_size: SIZE,
      human: human === 1 ? 'black' : 'white',
      started_at: rec.started_at,
      ended_at: new Date().toISOString(),
      result: res,
      winner: !result || result === 3 ? null : result === human ? 'human' : 'g2',
      undos: rec.undos,
      moves: moves.map(([x, y], i) => {
        const t = thoughts[i];
        const m = { ply: i + 1, x, y, coord: coord([x, y]), by: (i % 2 === 0 ? 1 : 2) === human ? 'human' : 'g2' };
        if (t) {
          m.g2 = t.book ? { reason: 'book', think_ms: t.think_ms }
            : { reason: t.reason, depth: t.depth, seldepth: t.seldepth, score: t.score, nodes: t.nodes, ms: t.ms,
                wall_ms: t.wall, think_ms: t.think_ms, pv: t.pv.map(([a, b]) => coord([a, b])) };
        }
        return m;
      }),
    };
  }

  // 保存先: claude.ai のアーティファクトの db (games/<あなたの id>/g/<対局 id>)。外部には送らない
  const store = (async () => {
    try {
      if (!window.claude || !window.claude.use) return null;
      const [db, user] = await Promise.all([window.claude.use('db'), window.claude.use('user')]);
      if (!db || !user) return null;
      const uid = await user.id();
      if (!uid) return null;
      return { db, uid };
    } catch (e) { return null; }
  })();
  let saving = Promise.resolve();
  const saveNoteEl = $('savenote');

  function saveGame() {
    const key = moves.length + ':' + result;
    if (!rec || moves.length === 0 || rec.saved_key === key) return;
    rec.saved_key = key;
    const data = gameRecord();
    const first = !rec.indexed;
    rec.indexed = true;
    saving = saving.then(async () => {
      const st = await store;
      if (!st) return;
      try {
        await st.db.doc(`games/${st.uid}/g/${data.id}`).set(data);
        if (first) await st.db.doc(`games/${st.uid}`).set({ updated_at: data.ended_at, last_game: data.id });
      } catch (e) {
        rec.indexed = !first && rec.indexed;   // 記録できない閲覧 (権限なし等) では黙って諦める
      }
    });
  }

  async function newGame() {
    game++;
    rule = Number($('rule').value);
    human = Number($('human').value);
    moves = []; thoughts = []; forbidden = []; result = 0; busy = false;
    rec = { id: new Date().toISOString().replace(/[-:.TZ]/g, '').slice(0, 14) + '-' + Math.random().toString(36).slice(2, 7),
            started_at: new Date().toISOString(), undos: 0, saved_key: '', indexed: false };
    infoEl.textContent = '';
    if (!worker) startWorker();
    setStatus('準備中…', '');
    const g = game;
    const s = await call({ type: 'new', rule, moves: [] });
    if (g !== game) return;
    if (s.version) { engineVersion = s.version; showEngine(s.version); }
    store.then((st) => {
      if (g === game) saveNoteEl.textContent = st ? '対局データ（棋譜と G2 の思考の数字）を G2 の改善に使います。' : '';
    });
    statsEl.hidden = true;
    logEl.replaceChildren(Object.assign(document.createElement('li'), { textContent: 'まだありません' }));
    logEl.dataset.empty = '1';
    applyState(s);
    updateStatus(); draw();
    aiMove();
  }

  async function undo() {
    if (busy || !moves.length) return;
    // AI の手と自分の手をまとめて戻す (あなたが白で 1 手目しかなければ戻さない)
    let n = sideToMove() === human ? 2 : 1;
    if (result && sideToMove() !== human) n = 1;     // 自分の手で終局した直後
    if (moves.length - n < (human === 2 ? 1 : 0)) n = moves.length - (human === 2 ? 1 : 0);
    if (n <= 0) return;
    const g = game;
    const s = await call({ type: 'undo', n });
    if (g !== game) return;
    moves.splice(moves.length - n, n);
    thoughts.splice(thoughts.length - n, n);
    rec.undos++;
    applyState(s);
    infoEl.textContent = '';
    updateStatus(); draw();
  }

  canvas.addEventListener('click', async (ev) => {
    const c = cellAt(ev);
    if (!c || !canPlay() || occupied(c[0], c[1])) return;
    if (human === 1 && forbidden.some(([a, b]) => a === c[0] && b === c[1])) {
      setStatus(`${coord(c)} は禁じ手です`, 'lose');
      return;
    }
    busy = true;
    const ok = await play(c[0], c[1]);
    busy = false;
    updateStatus(); draw();
    if (ok) aiMove();
  });
  canvas.addEventListener('mousemove', (ev) => {
    const c = cellAt(ev);
    if (String(c) !== String(hover)) { hover = c; draw(); }
  });
  canvas.addEventListener('mouseleave', () => { hover = null; draw(); });
  $('newgame').addEventListener('click', newGame);
  $('undo').addEventListener('click', undo);
  $('numbers').addEventListener('change', draw);
  window.addEventListener('resize', resize);
  window.matchMedia('(prefers-color-scheme: dark)').addEventListener?.('change', draw);

  resize();
  newGame();
})();
