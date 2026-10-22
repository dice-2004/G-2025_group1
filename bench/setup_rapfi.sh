#!/bin/bash
# 基準エンジン Rapfi (GPL, https://github.com/dhbloo/rapfi) を取得・ビルドして
# リポジトリの隣の rapfi-bin/ に置く。Rapfi のコードはこのリポジトリには入れない。
# 内蔵設定 (古典評価) で動く。NNUE を使う場合は rapfi-networks の重みと config.toml を
# rapfi-bin/ に置く (docs/ROADMAP.md の Phase 4 参照)。
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="${RAPFI_SRC:-$ROOT/../rapfi-src}"
BIN="${RAPFI_DIR:-$ROOT/../rapfi-bin}"

if [ ! -d "$SRC" ]; then
    git clone --depth 1 https://github.com/dhbloo/rapfi.git "$SRC"
fi
mkdir -p "$SRC/Rapfi/build"
cd "$SRC/Rapfi/build"
EXTRA=""
if grep -q avx2 /proc/cpuinfo 2>/dev/null; then EXTRA="-DUSE_AVX2=ON -DUSE_BMI2=ON"; fi
cmake .. -DCMAKE_BUILD_TYPE=Release $EXTRA > cmake.log
make -j"$(nproc)" > make.log
mkdir -p "$BIN"
cp pbrain-rapfi "$BIN/"
echo "Rapfi built: $BIN/pbrain-rapfi"
