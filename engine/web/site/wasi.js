// G2 の wasm (wasm32-wasi) を動かすための最小の WASI。ファイルは使わないので、時計・乱数・stderr への出力だけを実装する。
// ブラウザ (Worker の importScripts) と Node (require) の両方で使う。
(function (root) {
  function makeWasi(getMemory, log) {
    const ENOSYS = 52, EBADF = 8;
    const t0 = (typeof performance !== 'undefined') ? performance : { now: () => Date.now() };
    let lineBuf = '';
    const dec = new TextDecoder();
    const dv = () => new DataView(getMemory().buffer);
    const impl = {
      clock_time_get(id, _precision, ptr) {
        const ns = id === 0 ? BigInt(Date.now()) * 1000000n : BigInt(Math.round(t0.now() * 1e6));
        dv().setBigUint64(ptr, ns, true);
        return 0;
      },
      clock_res_get(_id, ptr) { dv().setBigUint64(ptr, 1000n, true); return 0; },
      random_get(buf, len) {
        const a = new Uint8Array(getMemory().buffer, buf, len);
        for (let i = 0; i < len; i += 65536) crypto.getRandomValues(a.subarray(i, Math.min(len, i + 65536)));
        return 0;
      },
      fd_write(fd, iovs, iovsLen, nwrittenPtr) {
        if (fd !== 1 && fd !== 2) return EBADF;
        const d = dv();
        let n = 0;
        for (let i = 0; i < iovsLen; i++) {
          const p = d.getUint32(iovs + i * 8, true), l = d.getUint32(iovs + i * 8 + 4, true);
          lineBuf += dec.decode(new Uint8Array(getMemory().buffer, p, l));
          n += l;
        }
        let k;
        while ((k = lineBuf.indexOf('\n')) >= 0) { if (log) log(lineBuf.slice(0, k)); lineBuf = lineBuf.slice(k + 1); }
        d.setUint32(nwrittenPtr, n, true);
        return 0;
      },
      environ_sizes_get(c, s) { const d = dv(); d.setUint32(c, 0, true); d.setUint32(s, 0, true); return 0; },
      args_sizes_get(c, s) { const d = dv(); d.setUint32(c, 0, true); d.setUint32(s, 0, true); return 0; },
      environ_get() { return 0; },
      args_get() { return 0; },
      sched_yield() { return 0; },
      proc_exit(code) { throw new Error('wasm proc_exit(' + code + ')'); },
    };
    return function importsFor(module) {
      const ns = {};
      for (const imp of WebAssembly.Module.imports(module)) {
        if (imp.kind !== 'function') continue;
        ns[imp.name] = impl[imp.name] || (() => ENOSYS);
      }
      return { wasi_snapshot_preview1: ns };
    };
  }

  // G2 エンジンの薄い包み。bytes: g2.wasm の中身
  async function loadG2(bytes, log) {
    const module = await WebAssembly.compile(bytes);
    let inst;
    const importsFor = makeWasi(() => inst.exports.memory, log);
    inst = await WebAssembly.instantiate(module, importsFor(module));
    if (inst.exports._initialize) inst.exports._initialize();
    return inst.exports;
  }

  const api = { makeWasi, loadG2 };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.G2Wasi = api;
})(typeof self !== 'undefined' ? self : this);
