#!/bin/bash
# Downloads the two transcribe.cpp GGUF models VibeFlow needs into models/:
#   - Qwen3-ASR 1.7B (dictation)          ~2.2 GB
#   - Whisper large-v3-turbo (Urdu/server) ~0.9 GB
set -e

cd "$(dirname "$0")/.."
mkdir -p models

for url in \
    "https://huggingface.co/handy-computer/Qwen3-ASR-1.7B-gguf/resolve/main/Qwen3-ASR-1.7B-Q8_0.gguf" \
    "https://huggingface.co/handy-computer/whisper-large-v3-turbo-gguf/resolve/main/whisper-large-v3-turbo-Q8_0.gguf"
do
    file="models/$(basename "$url")"
    if [ -f "$file" ]; then
        echo "Already present: $file"
        continue
    fi
    echo "Downloading $file ..."
    curl -fL --retry 3 -o "$file.part" "$url"
    mv "$file.part" "$file"
done

ls -lh models/*.gguf
