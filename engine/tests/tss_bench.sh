#!/bin/bash
# VCF / VCT の速度と再現率を測る (連珠の終盤 1507 局面、正解は Rapfi 30 万ノードの詰み評価)
#   engine/tests/tss_bench.sh [VCT のノード上限 ...]   既定: 10000 100000
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
TSS="$HERE/../build/g2-tss"
POS="$HERE/puzzles/renju_endgames.txt"
LAB="$HERE/puzzles/renju_endgames.rapfi"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
make -s -C "$HERE/.." >/dev/null
echo -n "VCF: "; "$TSS" solve renju vcf < "$POS" 2>&1 >"$TMP/vcf.out" | tr '\n' ' '; echo
ARGS=("VCF=$TMP/vcf.out")
for n in "${@:-10000 100000}"; do
  for m in $n; do
    echo -n "VCT ${m} nodes: "; "$TSS" solve renju vct 10 "$m" < "$POS" 2>&1 >"$TMP/vct$m.out" | tr '\n' ' '; echo
    ARGS+=("VCT${m}=$TMP/vct$m.out")
  done
done
python3 "$HERE/tss_compare.py" "$LAB" "${ARGS[@]}" --positions "$POS"
