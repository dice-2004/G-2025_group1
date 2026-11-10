#!/bin/sh
# G2 エンジンを WebAssembly にする (wasm32-wasi、libc は zig 同梱の wasi-libc)。
#   ZIG=/path/to/zig sh web/build.sh   (zig は `pip install ziglang` の ziglang/zig でよい)
# 出力: web/site/g2.wasm  (web/site/ をそのまま静的サイトとして公開する)
#
# wasm32 では long が 32 ビットなので、src/ の「上限なし」の意味の `1L << 40` (ノード数の上限) が
# 0 になり VCT が即座に打ち切られる。src/ は変えず、ビルド用の写しで LONG_MAX 相当に置き換える。
set -e
cd "$(dirname "$0")/.."
ZIG=${ZIG:-zig}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
cp src/*.c src/*.h "$TMP"/
sed -i 's/1L << 40/0x7fffffffL/g' "$TMP"/*.c
mkdir -p "$TMP/web" && cp web/g2_wasm.c "$TMP/web/"
sed -i 's#"\.\./src/#"../#' "$TMP/web/g2_wasm.c"
CORE=$(ls "$TMP"/*.c | grep -v -e /gomocup.c -e /class_entry.c)
mkdir -p web/site
$ZIG cc -target wasm32-wasi -O3 -std=c11 -D_POSIX_C_SOURCE=200809L \
    -mexec-model=reactor -Wl,--export-dynamic -Wl,--initial-memory=134217728 -Wl,--max-memory=536870912 \
    $CORE "$TMP/web/g2_wasm.c" -o web/site/g2.wasm
ls -l web/site/g2.wasm
