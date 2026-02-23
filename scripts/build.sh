#!/bin/bash
set -e

cd "$(dirname "$0")/.."

echo "=== VibeFlow Build ==="

cmake -B build \
    -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)" \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build -j$(sysctl -n hw.ncpu)

echo ""
echo "=== Bundling Qt frameworks ==="
macdeployqt build/VibeFlow.app -always-overwrite

# Copy model into app bundle Resources if present
MODEL_SRC="models/ggml-large-v3.bin"
MODEL_DST="build/VibeFlow.app/Contents/Resources/ggml-large-v3.bin"
if [ -f "$MODEL_SRC" ] && [ ! -f "$MODEL_DST" ]; then
    echo "Copying model into app bundle..."
    cp "$MODEL_SRC" "$MODEL_DST"
fi

echo ""
echo "=== Build complete ==="
echo "App: build/VibeFlow.app"
echo ""
echo "Install to /Applications:"
echo "  cp -R build/VibeFlow.app /Applications/"
