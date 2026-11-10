// CD のビルド後の確認: g2.wasm を Node で読み込み、何局面か考えさせて合法な手が返るかを見る。
//   node web/smoke_test.js web/site/g2.wasm
const fs = require('fs');
const path = require('path');
const { loadG2 } = require(path.join(__dirname, 'site', 'wasi.js'));

function fail(msg) { console.error('NG: ' + msg); process.exit(1); }

(async () => {
  const g = await loadG2(fs.readFileSync(process.argv[2] || path.join(__dirname, 'site', 'g2.wasm')), (s) => console.error('[g2] ' + s));
  for (const rule of [4, 0, 1]) {
    g.g2_new_game(rule, 15);
    // 序盤はブック、ブックを外れた局面は探索
    const opening = [[7, 7], [8, 6], [9, 9], [6, 8], [3, 12]];
    for (const [x, y] of opening) if (!g.g2_play(x, y)) fail('play');
    const t0 = Date.now();
    const m = g.g2_think(500);
    const ms = Date.now() - t0;
    if (m < 0) fail('no move');
    const x = m & 31, y = m >> 5;
    if (x > 14 || y > 14 || opening.some(([a, b]) => a === x && b === y)) fail(`illegal move ${x},${y}`);
    if (ms > 2000) fail(`too slow ${ms} ms`);
    console.log(`rule ${rule}: move ${x},${y} score ${g.g2_info(0)} depth ${g.g2_info(1)} nodes ${g.g2_info(2)}k ${g.g2_info(3)} ms (wall ${ms} ms)`);
  }
  // 勝ちを読む: 黒の四 (7,7 8,7 9,7 10,7) があり黒番なら 11,7 か 6,7 で五
  g.g2_new_game(0, 15);
  for (const [x, y] of [[7, 7], [7, 9], [8, 7], [8, 9], [9, 7], [9, 10], [10, 7], [3, 3]]) g.g2_play(x, y);
  const w = g.g2_think(300);
  if (!((w & 31) === 11 || (w & 31) === 6) || (w >> 5) !== 7) fail(`missed five: ${w & 31},${w >> 5}`);
  g.g2_play(w & 31, w >> 5);
  if (g.g2_result() !== 1) fail('result after five');
  // 連珠の禁じ手 (黒の三三): 7,7 白 0,0 / 8,8 白 0,1 / 7,9 白 0,2 / 9,7 ... 黒番で 7,8? 簡単な三三
  g.g2_new_game(4, 15);
  for (const [x, y] of [[6, 7], [0, 0], [7, 7], [0, 14], [8, 6], [14, 0], [8, 5], [14, 14]]) g.g2_play(x, y);
  // 黒 6,7 7,7 が横、8,6 8,5 が縦。8,7 に打つと横 6-7-8 と縦 5-6-7 の三三
  if (!g.g2_forbidden(8, 7)) fail('33 not forbidden');
  if (g.g2_forbidden(9, 9)) fail('false forbidden');
  // wasm に埋め込んだ版 ("G2 <コミット>") と、思考の情報 (読み筋・理由) が読めるか
  const mem = () => new Uint8Array(g.memory.buffer);
  const cstr = (p) => { let e = p; while (mem()[e]) e++; return Buffer.from(mem().subarray(p, e)).toString(); };
  const ver = cstr(g.g2_version());
  if (process.env.G2_COMMIT && ver !== 'G2 ' + process.env.G2_COMMIT) fail(`version ${ver} != ${process.env.G2_COMMIT}`);
  if (!/^G2 /.test(ver)) fail('version ' + ver);
  g.g2_new_game(0, 15);
  for (const [x, y] of [[7, 7], [8, 6], [9, 9], [6, 8], [3, 12]]) g.g2_play(x, y);
  g.g2_think(300);
  if (cstr(g.g2_reason()) !== 'search' || g.g2_pv(0) < 0 || g.g2_info(7) <= 0) fail('think info');
  console.log(`${ver}: reason ${cstr(g.g2_reason())} seldepth ${g.g2_info(5)} pv0 ${g.g2_pv(0) & 31},${g.g2_pv(0) >> 5}`);
  console.log('OK');
})().catch((e) => fail(e.stack || String(e)));
