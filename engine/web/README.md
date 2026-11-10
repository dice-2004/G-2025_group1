# G2 のブラウザ版

G2 エンジン (`engine/src`) を WebAssembly にして、ブラウザで対局できるようにした静的サイト。
エンジンのコードは変えず、`g2_wasm.c` (盤の操作と思考を関数として出す入口) を足してビルドしている。

- 公開 (予定の本命): GitHub Pages https://dice-2004.github.io/G-2025_group1/ 。`claude/dev` への push で GitHub Actions
  (`.github/workflows/pages.yml`) がビルド → Node で動作確認 → 公開する (`engine/web/pages.enabled` があるときだけ公開。PR はビルドと確認だけ)。
  対局データは Google Apps Script の記録先 (`collector/Code.gs`) に送る。組織外の人の対局も記録できる
- 公開 (今): claude.ai のアーティファクト https://claude.ai/artifact/CZdJbuBNsMpSExkEvfJ9uc 。Pages 版の公開と記録を確かめたら消してよい (dice 2026-11-10)。
  対局データはアーティファクトの db に記録するが、組織外の人の対局は記録できない

## GitHub Pages 版の記録先 (Google Apps Script)

`collector/Code.gs` を Google スプレッドシートに付けたウェブアプリとして公開し、その URL を `site/config.js` の
`G2_COLLECTOR_URL` に書く (URL はページに載るので秘密ではない)。スプレッドシート自体は非公開のまま。
スクリプトは `@OnlyCurrentDoc` で、触れるのはこのスプレッドシート 1 枚だけ。

- 書き込み (POST、誰でも): ページが G2 の 6 手目以降の手ごとと終局時に送る (text/plain、応答は読まない)。シート `games` に 1 局 1 行、同じ対局 id は上書き。
  弾くもの: 20 万字超、形の違うもの (盤外・重複・手番の食い違い・G2 の手に思考の情報がない・エンジン版の形・開始時刻が前後 1 日を外れる)、
  全体で 1 分 120 回超、新しい対局が 1 時間 60 局超、1 局の上書き 150 回超、記録 2 万局超
- 読み出し (GET、鍵が要る): `?key=<鍵>&since=<ISO 時刻>`。鍵はスクリプトのプロパティ `READ_KEY` (関数 `setup` で作られる。`resetKey` で作り直し)。
  鍵が違えば何も返さない
- 読み出しの手順: Claude のクラウド環境の環境変数 `G2_COLLECTOR_KEY` に鍵を入れておき (dice が設定)、
  `python3 engine/web/fetch_games.py [--since <ISO 時刻>] [--out engine/web/games.jsonl]`。1 行 1 局の JSONL (`games.jsonl` はコミットしない)。
  鍵はリポジトリ・メモリ・チャットに書かない
- 残るリスク: ログインなしのページなので、形の正しい偽の棋譜は (上限の範囲で) 送れてしまう。読み出したデータは手がかりとして使い、
  局面は G2・Rapfi で確かめ直す。止めたいときは Apps Script の「デプロイを管理」でアーカイブする

設置 (dice が 1 回だけ):
1. https://sheets.new で新しいスプレッドシートを作る
2. 「拡張機能」→「Apps Script」。エディタの中身を `collector/Code.gs` に置き換えて保存
3. 上の関数の選択で `setup` を選んで「実行」し、承認する。実行ログに出た「読み出しの鍵」を、Claude のプロジェクト設定 → クラウド環境 →
   環境変数に `G2_COLLECTOR_KEY=<鍵>` として入れる
4. 「デプロイ」→「新しいデプロイ」→ 種類「ウェブアプリ」、実行ユーザー「自分」、アクセス「全員」→ デプロイ。
   出てきた「ウェブアプリの URL」(`https://script.google.com/macros/s/.../exec`) を `site/config.js` に書く (Claude が行う)

`Code.gs` を変えたときは、貼り直して保存し、「デプロイを管理」→ 編集 → バージョン「新しいバージョン」で同じ URL のまま更新する。

## アーティファクトの更新手順

1. 変更をコミットする (wasm に埋め込むコミットを合わせるため、先にコミット)
2. `ZIG="python3 -m ziglang" sh engine/web/build.sh && node engine/web/smoke_test.js`
3. `python3 engine/web/make_artifact.py <出力.html> "$(date -u +%Y%m%d)-$(git rev-parse --short=7 HEAD)"`
4. Artifact ツールで `url: https://claude.ai/artifact/CZdJbuBNsMpSExkEvfJ9uc` に公開。`capabilities` は省略すると前の宣言 (下の db・user) を引き継ぐ

## 対局データ (自動収集)

(この節はアーティファクト版の db の話。Pages 版は上の記録先)

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
| `collector/Code.gs` | Pages 版の対局データの記録先 (Google Apps Script) |
| `fetch_games.py` | 記録先から対局を読み出して JSONL に保存 |
| `site/config.js` | 記録先の URL (`G2_COLLECTOR_URL`) |
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
