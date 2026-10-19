#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
CXX="${CXX:-clang++}"
COMMON="-std=c++17 -Wall -Wextra -Wpedantic"
echo "Compiler: $($CXX --version | head -n 1)"
echo "Building O0..."
$CXX $COMMON -O0 src/bfs.cpp -o bfs_O0
echo "Building O3..."
if $CXX $COMMON -O3 -mcpu=native src/bfs.cpp -o bfs_O3 2>/dev/null; then
  echo "Built O3 with -mcpu=native"
else
  echo "Compiler rejected -mcpu=native; building portable -O3 instead."
  $CXX $COMMON -O3 src/bfs.cpp -o bfs_O3
fi
ARGS="${ARGS:-100000 500000 12345 9}"
echo "Arguments: $ARGS"
echo "--- O0 ---"
./bfs_O0 $ARGS
echo "--- O3 ---"
./bfs_O3 $ARGS
