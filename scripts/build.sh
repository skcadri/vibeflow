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
macdeployqt build/VibeFlow.app -always-overwrite 2>&1 || true

# Fix rpaths: macdeployqt doesn't rewrite absolute Homebrew paths in the main binary
echo "Fixing framework rpaths..."
BINARY=build/VibeFlow.app/Contents/MacOS/VibeFlow
otool -L "$BINARY" | grep /opt/homebrew | awk '{print $1}' | while read OLD_PATH; do
    FRAMEWORK=$(echo "$OLD_PATH" | sed 's|.*/\(Qt[^/]*\.framework/.*\)|\1|')
    NEW_PATH="@executable_path/../Frameworks/$FRAMEWORK"
    install_name_tool -change "$OLD_PATH" "$NEW_PATH" "$BINARY" 2>/dev/null
done

# Re-sign everything (macdeployqt + install_name_tool invalidate signatures)
echo "Code signing..."
cd build/VibeFlow.app
find . \( -name "*.dylib" -o -name "*.framework" \) -exec codesign --force --sign - {} \; 2>/dev/null
codesign --force --sign - Contents/MacOS/VibeFlow
codesign --force --deep --sign - .
cd ../..

# Copy model into app bundle Resources if present
MODEL_SRC="models/ggml-large-v3.bin"
MODEL_DST="build/VibeFlow.app/Contents/Resources/ggml-large-v3.bin"
if [ -f "$MODEL_SRC" ] && [ ! -f "$MODEL_DST" ]; then
    echo "Copying model into app bundle (~3GB)..."
    cp "$MODEL_SRC" "$MODEL_DST"
    # Re-sign after adding model
    codesign --force --deep --sign - build/VibeFlow.app
fi

echo ""
echo "=== Build complete ==="
echo "App: build/VibeFlow.app"
echo ""
echo "Install:"
echo "  rm -rf /Applications/VibeFlow.app && cp -R build/VibeFlow.app /Applications/"
