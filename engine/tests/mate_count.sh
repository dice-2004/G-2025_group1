#!/bin/bash
# 探索の鋭さの目安: tests/mates_renju.txt の局面 (手番側に Rapfi が見つけた詰みがある) を
# 時間を決めて探索し、詰みの点数を返した局面の数と平均の深さを出す。
#   [G2_PARAMS="name=val,..."] engine/tests/mate_count.sh [ms] [engine/build/g2-test] [threads]
ms=${1:-1000}
bin=${2:-$(dirname "$0")/../build/g2-test}
th=${3:-1}
tmp=$(mktemp -d)
grep -v '^#' "$(dirname "$0")/mates_renju.txt" | cut -f1 > "$tmp/all"
# 複数スレッドなら 1 本ずつ、1 スレッドなら 4 並列
if [ "$th" -gt 1 ]; then cp "$tmp/all" "$tmp/part.a"; else split -n l/4 "$tmp/all" "$tmp/part."; fi
for f in "$tmp"/part.*; do G2_PARAMS="$G2_PARAMS" "$bin" timed 4 "$ms" "$th" < "$f" > "$f.out" & done
wait
cat "$tmp"/part.*.out | awk -v ms="$ms" '{n++; d+=$3; if ($2 > 29000) m++} END {printf "mates %d / %d, avg depth %.2f (%d ms)\n", m, n, d/n, ms}'
rm -rf "$tmp"
