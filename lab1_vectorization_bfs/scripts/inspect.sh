
#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

CXX="${CXX:-clang++}"
mkdir -p artifacts

{
  echo "Date: $(date)"
  echo "OS:"
  sw_vers 2>/dev/null || uname -a
  echo "Architecture: $(uname -m)"
  echo "Compiler:"
  "$CXX" --version | head -n 3
  echo "CPU:"
  sysctl -n machdep.cpu.brand_string 2>/dev/null || true
} | tee artifacts/system.txt

VERSION="$("$CXX" --version 2>&1 | head -n 1)"

if echo "$VERSION" | grep -qi "clang"; then
  echo "Using Clang vectorization diagnostics"

  "$CXX" -std=c++17 -O3 -mcpu=native \
    -Rpass=loop-vectorize \
    -Rpass-missed=loop-vectorize \
    -Rpass-analysis=loop-vectorize \
    src/bfs.cpp -o artifacts/bfs_vec \
    2> artifacts/vec-report.txt

  "$CXX" -std=c++17 -O3 -mcpu=native \
    -S src/bfs.cpp -o artifacts/bfs.s
else
  echo "Using GCC vectorization diagnostics"

  "$CXX" -std=c++17 -O3 \
    -fopt-info-vec-optimized=artifacts/vec.opt.txt \
    -fopt-info-vec-missed=artifacts/vec.missed.txt \
    src/bfs.cpp -o artifacts/bfs_vec

  "$CXX" -std=c++17 -O3 -S src/bfs.cpp -o artifacts/bfs.s
fi

echo "Artifacts written to artifacts/"
