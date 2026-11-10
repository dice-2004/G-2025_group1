/** @OnlyCurrentDoc */  // このスクリプトが付いているスプレッドシートだけを扱う (Google の承認もこの 1 枚だけになる)
// G2 の対局データの記録先 (Google Apps Script のウェブアプリ)。GitHub Pages 版のページが棋譜を POST し、
// 強化スレッドが GET で読み出す。Google スプレッドシートに 1 局 1 行で保存する (同じ対局 id は上書き)。
// 設置手順は engine/web/README.md の「GitHub Pages 版の記録先」。秘密情報は使わない (URL を知っていれば誰でも書けて読める)。

var SHEET = 'games';
var HEADER = ['id', 'updated_at', 'engine', 'rule', 'human', 'result', 'winner', 'moves', 'json'];
var MAX_BODY = 200000;      // 1 局の上限 (文字)。セルの上限は 5 万字なので、超える分は読み筋を削る
var MAX_CELL = 49000;

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

function doPost(e) {
  var body = e && e.postData ? e.postData.contents : '';
  if (!body || body.length > MAX_BODY) return json_({ ok: false, error: 'size' });
  var g;
  try { g = JSON.parse(body); } catch (err) { return json_({ ok: false, error: 'json' }); }
  if (!g || g.format !== 'g2-game/1' || !/^[0-9A-Za-z-]{6,40}$/.test(String(g.id)) || !Array.isArray(g.moves) || g.moves.length > 400) {
    return json_({ ok: false, error: 'format' });
  }
  var text = JSON.stringify(g);
  if (text.length > MAX_CELL) {
    g.moves.forEach(function (m) { if (m.g2 && m.g2.pv) m.g2.pv = m.g2.pv.slice(0, 2); });
    text = JSON.stringify(g);
    if (text.length > MAX_CELL) return json_({ ok: false, error: 'size' });
  }
  var row = [g.id, new Date().toISOString(), String(g.engine || ''), String(g.rule || ''), String(g.human || ''),
             String(g.result || ''), String(g.winner || ''), g.moves.length, text];
  var lock = LockService.getScriptLock();
  lock.waitLock(10000);
  try {
    var sh = sheet_();
    var hit = sh.getRange('A:A').createTextFinder(g.id).matchEntireCell(true).findNext();
    if (hit && hit.getRow() > 1) sh.getRange(hit.getRow(), 1, 1, row.length).setValues([row]);
    else sh.appendRow(row);
  } finally {
    lock.releaseLock();
  }
  return json_({ ok: true });
}

// GET ?since=<ISO 時刻> : その時刻より後に記録 (更新) された対局を返す。since なしなら全部
function doGet(e) {
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
