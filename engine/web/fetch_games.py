#!/usr/bin/env python3
"""GitHub Pages 版で記録した対局 (collector/Code.gs) を読み出して JSONL に保存する。
   python3 engine/web/fetch_games.py [--since 2026-11-10T00:00:00Z] [--out engine/web/games.jsonl]
記録先の URL は site/config.js の G2_COLLECTOR_URL から、読み出しの鍵は環境変数 G2_COLLECTOR_KEY から読む
(鍵は Claude のプロジェクトのクラウド環境の環境変数に dice が設定する。鍵をリポジトリ・メモリ・チャットに書かないこと)。
1 行 1 局 (形式 g2-game/1、engine/web/README.md)。--out が既にあれば同じ id の局を新しいもので置き換えて追記する。"""
import argparse, json, os, re, sys, urllib.parse, urllib.request

here = os.path.dirname(os.path.abspath(__file__))
ap = argparse.ArgumentParser()
ap.add_argument('--since', default='')
ap.add_argument('--out', default=os.path.join(here, 'games.jsonl'))
ap.add_argument('--url', default='')
a = ap.parse_args()

url = a.url
if not url:
    m = re.search(r"G2_COLLECTOR_URL\s*=\s*'([^']*)'", open(os.path.join(here, 'site', 'config.js'), encoding='utf-8').read())
    url = m.group(1) if m else ''
if not url:
    sys.exit('G2_COLLECTOR_URL が空です (site/config.js)')
key = os.environ.get('G2_COLLECTOR_KEY', '')
if not key:
    sys.exit('環境変数 G2_COLLECTOR_KEY (読み出しの鍵) がありません')
q = {'key': key}
if a.since:
    q['since'] = a.since
url += ('&' if '?' in url else '?') + urllib.parse.urlencode(q)
with urllib.request.urlopen(url, timeout=60) as r:   # Apps Script は googleusercontent へリダイレクトする (urllib が追う)
    res = json.load(r)
if not res.get('ok'):
    sys.exit('記録先がエラーを返しました: %s' % res.get('error'))

games = {}
if os.path.exists(a.out):
    for line in open(a.out, encoding='utf-8'):
        if line.strip():
            g = json.loads(line)
            games[g['id']] = g
for g in res['games']:
    games[g['id']] = g
with open(a.out, 'w', encoding='utf-8') as f:
    for g in sorted(games.values(), key=lambda g: g.get('started_at', '')):
        f.write(json.dumps(g, ensure_ascii=False) + '\n')
fin = [g for g in games.values() if g.get('result')]
print('%d 局を受信、%s に計 %d 局 (終局 %d、G2 の勝ち %d)' % (res['count'], a.out, len(games), len(fin),
      sum(1 for g in fin if g.get('winner') == 'g2')))
