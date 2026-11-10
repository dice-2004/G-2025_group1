/** @OnlyCurrentDoc */  // このスクリプトが付いているスプレッドシートだけを扱う (Google の承認もこの 1 枚だけになる)
// G2 の対局データの記録先 (Google Apps Script のウェブアプリ)。GitHub Pages 版のページが棋譜を POST し、
// 強化スレッドが GET で読み出す。Google スプレッドシートに 1 局 1 行で保存する (同じ対局 id は上書き)。
// 設置手順は engine/web/README.md の「GitHub Pages 版の記録先」。
// - 書き込み (POST): 公開 (ページに鍵は置けないため)。形・大きさ・回数を確かめて、筋の通らないものは弾く
// - 読み出し (GET): 読み出しの鍵 (スクリプトのプロパティ READ_KEY) が要る。鍵は setup() を 1 回実行すると作られ、ログに出る

var SHEET = 'games';
var HEADER = ['id', 'updated_at', 'engine', 'rule', 'human', 'result', 'winner', 'moves', 'json', 'updates'];
var MAX_BODY = 200000;      // 1 局の上限 (文字)。セルの上限は 5 万字なので、超える分は読み筋を削る
var MAX_CELL = 49000;
var MAX_ROWS = 20000;       // 記録する対局の上限 (これを超えたら新しい対局は受けない)
var MAX_WRITES_PER_MIN = 120;   // 全体の書き込み回数の上限 (1 分)
var MAX_NEW_PER_HOUR = 60;      // 新しい対局の上限 (1 時間)
var MAX_UPDATES_PER_GAME = 150; // 1 局あたりの上書き回数の上限

// 読み出しの鍵を作る (なければ)。Apps Script のエディタで関数 setup を選んで「実行」し、ログに出た鍵を使う。
// 鍵を変えたいときは resetKey を実行する
function setup() {
  var props = PropertiesService.getScriptProperties();
  var key = props.getProperty('READ_KEY');
  if (!key) {
    key = Utilities.getUuid().replace(/-/g, '') + Utilities.getUuid().replace(/-/g, '');
    props.setProperty('READ_KEY', key);
  }
  sheet_();
  Logger.log('読み出しの鍵: ' + key);
}

function resetKey() {
  PropertiesService.getScriptProperties().deleteProperty('READ_KEY');
  setup();
}

// 回数の上限。name の窓 (秒) の中で limit を超えたら false
function allow_(name, windowSec, limit) {
  var cache = CacheService.getScriptCache();
  var slot = name + ':' + Math.floor(Date.now() / 1000 / windowSec);
  var n = Number(cache.get(slot) || 0) + 1;
  cache.put(slot, String(n), windowSec * 2);
  return n <= limit;
}

function sheet_() {
  var ss = SpreadsheetApp.getActiveSpreadsheet();
  var sh = ss.getSheetByName(SHEET);
  if (!sh) {
    sh = ss.insertSheet(SHEET);
    sh.appendRow(HEADER);
    sh.setFrozenRows(1);
  }
  return sh;
}

function json_(obj) {
  return ContentService.createTextOutput(JSON.stringify(obj)).setMimeType(ContentService.MimeType.JSON);
}

// いたずらの書き込みを弾くための形の確認 (ページが作る棋譜として筋が通っているか)。
// 鍵はページに置けない (誰でも見える) ので、内容で確かめる
function valid_(g) {
  if (!g || g.format !== 'g2-game/1' || !/^[0-9]{14}-[0-9a-z]{3,8}$/.test(String(g.id))) return false;
  if (!/^G2 [0-9a-f]{7,40}$/.test(String(g.engine))) return false;
  // 対局 id の先頭は開始時刻 (UTC の YYYYMMDDhhmmss)。今から前後 1 日を外れるものは弾く
  var t = String(g.id);
  var started = Date.UTC(+t.slice(0, 4), +t.slice(4, 6) - 1, +t.slice(6, 8), +t.slice(8, 10), +t.slice(10, 12), +t.slice(12, 14));
  if (!(Math.abs(Date.now() - started) < 86400000)) return false;
  if (['renju', 'freestyle', 'standard'].indexOf(g.rule) < 0 || g.board_size !== 15) return false;
  if (['black', 'white'].indexOf(g.human) < 0) return false;
  if ([null, 'black', 'white', 'draw'].indexOf(g.result) < 0) return false;
  if (!Array.isArray(g.moves) || g.moves.length < 1 || g.moves.length > 225) return false;
  var seen = {}, g2moves = 0;
  for (var i = 0; i < g.moves.length; i++) {
    var m = g.moves[i];
    if (!m || m.ply !== i + 1 || !(m.x >= 0 && m.x < 15 && m.y >= 0 && m.y < 15) || m.x % 1 || m.y % 1) return false;
    var k = m.x * 15 + m.y;
    if (seen[k]) return false;
    seen[k] = 1;
    var black = i % 2 === 0;
    var humanMove = (g.human === 'black') === black;
    if (m.by !== (humanMove ? 'human' : 'g2')) return false;
    if (!humanMove) {
      if (!m.g2 || typeof m.g2.reason !== 'string') return false;
      g2moves++;
    }
  }
  return g2moves > 0;
}

function doPost(e) {
  var body = e && e.postData ? e.postData.contents : '';
  if (!body || body.length > MAX_BODY) return json_({ ok: false, error: 'size' });
  var g;
  try { g = JSON.parse(body); } catch (err) { return json_({ ok: false, error: 'json' }); }
  if (!valid_(g)) return json_({ ok: false, error: 'format' });
  var text = JSON.stringify(g);
  if (text.length > MAX_CELL) {
    g.moves.forEach(function (m) { if (m.g2 && m.g2.pv) m.g2.pv = m.g2.pv.slice(0, 2); });
    text = JSON.stringify(g);
    if (text.length > MAX_CELL) return json_({ ok: false, error: 'size' });
  }
  var lock = LockService.getScriptLock();
  lock.waitLock(10000);
  try {
    if (!allow_('w', 60, MAX_WRITES_PER_MIN)) return json_({ ok: false, error: 'rate' });
    var sh = sheet_();
    var hit = sh.getRange('A:A').createTextFinder(g.id).matchEntireCell(true).findNext();
    var at = hit && hit.getRow() > 1 ? hit.getRow() : 0;
    var updates = 0;
    if (at) {
      updates = Number(sh.getRange(at, HEADER.length).getValue() || 0) + 1;
      if (updates > MAX_UPDATES_PER_GAME) return json_({ ok: false, error: 'rate' });
    } else {
      if (sh.getLastRow() - 1 >= MAX_ROWS || !allow_('n', 3600, MAX_NEW_PER_HOUR)) return json_({ ok: false, error: 'rate' });
    }
    var row = [g.id, new Date().toISOString(), String(g.engine || ''), String(g.rule || ''), String(g.human || ''),
               String(g.result || ''), String(g.winner || ''), g.moves.length, text, updates];
    if (at) sh.getRange(at, 1, 1, row.length).setValues([row]);
    else sh.appendRow(row);
  } finally {
    lock.releaseLock();
  }
  return json_({ ok: true });
}

// GET ?key=<読み出しの鍵>&since=<ISO 時刻> : その時刻より後に記録 (更新) された対局を返す。since なしなら全部。
// 鍵が違えば何も返さない
function doGet(e) {
  var key = PropertiesService.getScriptProperties().getProperty('READ_KEY');
  if (!key || !e || !e.parameter || e.parameter.key !== key) return json_({ ok: false, error: 'key' });
  var since = e && e.parameter && e.parameter.since ? String(e.parameter.since) : '';
  var sh = sheet_();
  var n = sh.getLastRow() - 1;
  var games = [];
  if (n > 0) {
    var rows = sh.getRange(2, 1, n, HEADER.length).getValues();
    rows.forEach(function (r) {
      var updated = r[1] instanceof Date ? r[1].toISOString() : String(r[1]);
      if (since && updated <= since) return;
      try {
        var g = JSON.parse(r[8]);
        g.recorded_at = updated;
        games.push(g);
      } catch (err) { /* 壊れた行は飛ばす */ }
    });
  }
  return json_({ ok: true, count: games.length, games: games });
}
