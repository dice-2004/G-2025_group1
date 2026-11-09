#!/bin/bash
# 1 手の時間を決めたときの到達深さと nps: tests/speed_positions.txt の 16 局面 (+ 詰みの物差しの先頭 48 局面) を探索。4 並列。
#   [G2_PARAMS="name=val,..."] engine/tests/depth_bench.sh [ms] [engine/build/g2-test]
ms=${1:-1000}
bin=${2:-$(dirname "$0")/../build/g2-test}
tmp=$(mktemp -d)
{ cat "$(dirname "$0")/speed_positions.txt"; grep -v '^#' "$(dirname "$0")/mates_renju.txt" | cut -f1 | head -48; } > "$tmp/all"
split -n l/4 "$tmp/all" "$tmp/part."
for f in "$tmp"/part.*; do G2_PARAMS="$G2_PARAMS" "$bin" timed 4 "$ms" 1 < "$f" > "$f.out" & done
wait
cat "$tmp"/part.*.out | awk -v ms="$ms" '{n++; d+=$3; nodes+=$4; t+=$5} END {printf "avg depth %.2f, nps %.0f (%d positions, %d ms)\n", d/n, nodes/t*1000, n, ms}'
rm -rf "$tmp"
