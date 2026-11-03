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

# Prefer a stable signing identity so macOS TCC (microphone permission)
# can persist across rebuilds. Fall back to ad-hoc only if needed.
SIGN_IDENTITY="${CODESIGN_IDENTITY:-}"
if [ -z "$SIGN_IDENTITY" ]; then
    if security find-identity -v -p codesigning | grep -q '"VibeFlow Dev"'; then
        SIGN_IDENTITY="VibeFlow Dev"
    else
        SIGN_IDENTITY="-"
    fi
fi

echo "Signing identity: $SIGN_IDENTITY"
if [ "$SIGN_IDENTITY" = "-" ]; then
    echo "WARNING: using ad-hoc signing. macOS may not persist microphone permission across rebuilds."
    echo "Set CODESIGN_IDENTITY to a stable certificate (for example: CODESIGN_IDENTITY=\"VibeFlow Dev\")."
fi

cd build/VibeFlow.app
find . \( -name "*.dylib" -o -name "*.framework" \) -exec codesign --force --sign "$SIGN_IDENTITY" {} \; 2>/dev/null
codesign --force --sign "$SIGN_IDENTITY" Contents/MacOS/VibeFlow
codesign --force --deep --sign "$SIGN_IDENTITY" .
cd ../..

# Copy models into app bundle Resources if present (APFS clone when possible)
for MODEL in Qwen3-ASR-1.7B-Q8_0.gguf whisper-large-v3-turbo-Q8_0.gguf; do
    MODEL_SRC="models/$MODEL"
    MODEL_DST="build/VibeFlow.app/Contents/Resources/$MODEL"
    if [ -f "$MODEL_SRC" ] && [ ! -f "$MODEL_DST" ]; then
        echo "Copying $MODEL into app bundle..."
        cp -c "$MODEL_SRC" "$MODEL_DST" 2>/dev/null || cp "$MODEL_SRC" "$MODEL_DST"
        MODELS_ADDED=1
    fi
done
if [ -n "$MODELS_ADDED" ]; then
    codesign --force --deep --sign "$SIGN_IDENTITY" build/VibeFlow.app
fi

echo ""
echo "=== Build complete ==="
echo "App: build/VibeFlow.app"
echo ""
echo "Install:"
echo "  rm -rf /Applications/VibeFlow.app && cp -R build/VibeFlow.app /Applications/"
