# G2 のブラウザ版

G2 エンジン (`engine/src`) を WebAssembly にして、ブラウザで対局できるようにした静的サイト。
エンジンのコードは変えず、`g2_wasm.c` (盤の操作と思考を関数として出す入口) を足してビルドしている。

- 公開 (本命): claude.ai のアーティファクト https://claude.ai/artifact/CZdJbuBNsMpSExkEvfJ9uc (リンクを知っていれば開ける)。
  1 ファイル版 (wasm を base64 で埋め込み) を `make_artifact.py` で作り、Artifact ツールで同じ URL に公開し直す。自動では更新されないので節目ごとに更新する
- CI: `claude/dev` への push と PR で、GitHub Actions (`.github/workflows/pages.yml`) が wasm をビルドして Node で動作確認する。
  GitHub Pages への公開はリポジトリ変数 `PAGES_ENABLED=true` のときだけ (dice の判断で今は使わない)

## アーティファクトの更新手順

1. 変更をコミットする (wasm に埋め込むコミットを合わせるため、先にコミット)
2. `ZIG="python3 -m ziglang" sh engine/web/build.sh && node engine/web/smoke_test.js`
3. `python3 engine/web/make_artifact.py <出力.html> "$(date -u +%Y%m%d)-$(git rev-parse --short=7 HEAD)"`
4. Artifact ツールで `url: https://claude.ai/artifact/CZdJbuBNsMpSExkEvfJ9uc` に公開。`capabilities` は省略すると前の宣言 (下の db・user) を引き継ぐ

## 対局データ (自動収集)

アーティファクト版は、対局の棋譜と G2 の思考の数字を、アーティファクトの db (claude.ai のアーティファクト内の保存領域) に自動で記録する。
ページには「対局データ（棋譜と G2 の思考の数字）を G2 の改善に使います。」と表示する。名前・メールなどの個人情報は記録しない
(保存先のパスに claude.ai の不透明な利用者 id が入るだけ)。

- 記録する時: G2 が 6 手目以降の手を打つたびと、終局したとき (同じ対局は同じ文書に上書き)。途中で閉じた対局も最後の G2 の手まで残る
- 記録できる人: db に書けるのは Contributor 以上 (アーティファクトの持ち主と、組織のメンバー)。
  組織の外からリンクで開いた人・サインインしていない人の対局は記録されない (claude.ai の仕組み上の制限: db は読むだけ、room にはつながれず、
  アーティファクトのページは他のホストへの通信を止められている)。組織外の対局も集めたい場合の案は、GitHub Pages 版から外部の記録先
  (Cloudflare Workers・Supabase など) に送ること (Pages の有効化と記録先のアカウントが dice に必要。未着手)
- 宣言 (`capabilities`):
  `{"db": {"rules": [{"path": "games", "read": "admin", "write": "admin"}, {"path": "games/{self}", "read": "interact", "write": "interact"}]}, "user": {}}`
  (各自は自分の `games/<id>/...` だけ書ける。全員分を読めるのは持ち主と Editor)
- 置き場所: `games/<利用者 id>` (目印の文書: `updated_at`, `last_game`)、`games/<利用者 id>/g/<対局 id>` (1 局 1 文書)

### 取り出し方 (強化スレッド向け)

Claude の `ArtifactData` ツールで、`url` に上のアーティファクトを渡して読む (dice の権限で読むので全員分が見える)。

1. `action: "list"`, `collection: "games"` → 利用者ごとの目印の文書 (id が利用者 id)
2. `action: "list"`, `collection: "games/<利用者 id>/g"` → その人の対局 (ページ送りは結果の続きの印を使う)

### 1 局の形式 (`format: "g2-game/1"`)

| フィールド | 内容 |
|---|---|
| `id` | 対局 id (開始時刻 + 乱数) |
| `engine` | wasm が返した版 `"G2 <コミット>"` |
| `page_build` | ページのビルド名 (日付-コミット) |
| `rule` / `gomocup_rule` | `"renju"`/4, `"freestyle"`/0, `"standard"`/1 |
| `board_size` | 15 |
| `human` | 人の石 `"black"` / `"white"` |
| `started_at` / `ended_at` | ISO 時刻 (`ended_at` は最後に記録した時刻) |
| `result` | `"black"` / `"white"` / `"draw"` / `null` (終わっていない) |
| `winner` | `"human"` / `"g2"` / `null` |
| `undos` | 「待った」の回数 (手順は待ったの後の最終形だけ) |
| `moves[]` | `ply` (1 から), `x`, `y` (左上が 0,0。Gomocup・pbrain-g2 と同じ座標), `coord` (例 `H8`、I を飛ばす列 + 下から数えた行), `by` (`"human"`/`"g2"`) |
| `moves[].g2` | G2 の手だけ: `reason` (`search`/`vcf`/`vct`/`ts`/`book`/`five`/`block-five`/`flex4`/`only-move`/`verified`/`lost` など), `depth`, `seldepth`, `score` (G2 から見た評価、±29000 超は詰み), `nodes`, `ms` (探索の時間), `wall_ms` (Worker で測った時間), `think_ms` (持ち時間の設定), `pv` (読み筋、`coord` 表記)。ブックの手は `reason: "book"` と `think_ms` だけ |

`x`,`y` の手順はそのまま `pbrain-g2` の `BOARD` や `g2-test` に渡せる。

## 構成

| ファイル | 役割 |
|---|---|
| `g2_wasm.c` | wasm の入口 (`g2_new_game` / `g2_play` / `g2_undo` / `g2_forbidden` / `g2_result` / `g2_think` / `g2_info` / `g2_pv` / `g2_reason` / `g2_version`)。`g2_version` はビルド元のコミット (`build.sh` が `-DG2_COMMIT` で埋め込む) |
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
