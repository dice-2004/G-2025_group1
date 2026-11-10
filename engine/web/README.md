# G2 のブラウザ版

G2 エンジン (`engine/src`) を WebAssembly にして、ブラウザで対局できるようにした静的サイト。
エンジンのコードは変えず、`g2_wasm.c` (盤の操作と思考を関数として出す入口) を足してビルドしている。

- 公開: `claude/dev` に push すると GitHub Actions (`.github/workflows/pages.yml`) が
  ビルド → Node で動作確認 → GitHub Pages に公開する。URL は https://dice-2004.github.io/G-2025_group1/
- pull request ではビルドと動作確認だけ行う
- claude.ai のアーティファクト版 (1 ファイル、wasm を base64 で埋め込み): https://claude.ai/artifact/CZdJbuBNsMpSExkEvfJ9uc
  (自動では更新されない。節目ごとに `python3 engine/web/make_artifact.py <出力.html> <ビルド名>` で作り直して公開し直す。
  見られるのは持ち主と共有した人だけ)

## 構成

| ファイル | 役割 |
|---|---|
| `g2_wasm.c` | wasm の入口 (`g2_new_game` / `g2_play` / `g2_undo` / `g2_forbidden` / `g2_result` / `g2_think` / `g2_info`) |
| `build.sh` | `zig cc -target wasm32-wasi` でビルド (`site/g2.wasm`)。wasm32 は long が 32 ビットなので、ビルド用の写しで `1L << 40` を置き換える |
| `smoke_test.js` | CI の動作確認 (3 ルールで合法手を返す・五を打つ・連珠の三三を禁じ手と判定する) |
| `make_artifact.py` | `site/` を 1 つの HTML にまとめる (アーティファクト用) |
| `site/` | 公開する静的サイト。`index.html` + `app.js` (盤面 UI) + `worker.js` (Web Worker で思考) + `wasi.js` (最小の WASI) |

思考は 1 スレッド (Lazy SMP なし)、置換表 32MB。

## 手元で動かす

```sh
pip install ziglang            # zig (C → wasm のコンパイラ) が入る
ZIG="python3 -m ziglang" sh engine/web/build.sh
node engine/web/smoke_test.js
cd engine/web/site && python3 -m http.server 8000   # http://localhost:8000/
```
