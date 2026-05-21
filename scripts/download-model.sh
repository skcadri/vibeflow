#!/bin/bash
set -e

cd "$(dirname "$0")/.."

MODEL_DIR="models"
MODEL_FILE="$MODEL_DIR/ggml-large-v3-turbo.bin"

if [ -f "$MODEL_FILE" ]; then
    echo "Model already exists: $MODEL_FILE"
    exit 0
fi

mkdir -p "$MODEL_DIR"

echo "Downloading whisper large-v3-turbo model (~1.6GB)..."
echo "This will take a while on first run."

# Use whisper.cpp's download script if available
if [ -f "deps/whisper.cpp/models/download-ggml-model.sh" ]; then
    cd deps/whisper.cpp
    bash models/download-ggml-model.sh large-v3-turbo
    mv models/ggml-large-v3-turbo.bin "../../$MODEL_FILE"
    cd ../..
else
    # Direct download from Hugging Face
    curl -L -o "$MODEL_FILE" \
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3-turbo.bin"
fi

echo "Model downloaded: $MODEL_FILE"
ls -lh "$MODEL_FILE"
