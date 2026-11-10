#!/usr/bin/env python3
"""site/ を 1 つの HTML にまとめる (claude.ai のアーティファクト用。wasm は base64 で埋め込む)。
   python3 engine/web/make_artifact.py <出力.html> [ビルド名]
先に web/build.sh で site/g2.wasm を作っておくこと。"""
import base64, json, os, re, sys

site = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'site')
read = lambda f: open(os.path.join(site, f), encoding='utf-8').read()

html = read('index.html')
build = sys.argv[2] if len(sys.argv) > 2 else 'artifact'
html = html.replace('__BUILD__', build)
# アーティファクトは <!doctype>/<html>/<head>/<body> を自分で付けるので外す
head = re.search(r'<head>(.*)</head>', html, re.S).group(1)
body = re.search(r'<body>(.*)</body>', html, re.S).group(1)
head = re.sub(r'<meta charset[^>]*>\s*|<meta name="viewport"[^>]*>\s*', '', head)
inline = {
    'worker': read('wasi.js') + '\n' + read('worker.js'),
    'wasm': base64.b64encode(open(os.path.join(site, 'g2.wasm'), 'rb').read()).decode(),
}
app = read('app.js').replace('</script', '<\\/script')
body = re.sub(r'<script src="config\.js[^"]*"></script>\s*', '', body)
body = re.sub(r'<script src="app\.js[^"]*"></script>',
              lambda _: '<script>window.G2_INLINE = ' + json.dumps(inline).replace('</', '<\\/') + ';</script>\n<script>\n' + app + '\n</script>', body)
out = head.strip() + '\n' + body.strip() + '\n'
open(sys.argv[1], 'w', encoding='utf-8').write(out)
print(sys.argv[1], len(out), 'bytes')
