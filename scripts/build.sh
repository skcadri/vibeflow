#!/bin/bash
set -e

cd "$(dirname "$0")/.."

echo "=== VibeFlow Build ==="

cmake -B build \
    -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)" \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build -j$(sysctl -n hw.ncpu)

echo ""
echo "=== Build complete ==="
echo "Run: ./build/VibeFlow.app/Contents/MacOS/VibeFlow"
