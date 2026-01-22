# MedASR Setup Guide

## Quick Start (Python)

### 1. Install CUDA PyTorch

```bash
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu121
```

### 2. Install Dependencies

```bash
pip install -r requirements.txt
```

### 3. Run the App

```bash
python -m src.medasr
```

**First run:** Will download the MedASR model (~400MB) - wait for "MedASR model loaded successfully"

### 4. Use It

1. Press **Ctrl+Win** to start recording (red bubble appears)
2. Speak your text
3. Release **Ctrl+Win** to transcribe (bubble turns yellow, then types your text)
4. Press **Escape** while recording to cancel

---

## Building Standalone .exe

### 1. Install PyInstaller

```bash
pip install pyinstaller
```

### 2. Build

```bash
pyinstaller --name "MedASR" ^
    --windowed ^
    --onedir ^
    --add-data "config;config" ^
    --hidden-import "torch" ^
    --hidden-import "transformers" ^
    --collect-all "torch" ^
    --collect-all "transformers" ^
    src\medasr\__main__.py
```

### 3. Run

```bash
dist\MedASR\MedASR.exe
```

**Output:** `dist/MedASR/` folder (~2-3GB)

---

## Troubleshooting

### GPU Not Detected

```bash
python -c "import torch; print(f'CUDA: {torch.cuda.is_available()}')"
```

If False, reinstall PyTorch with CUDA support.

### Audio Device Not Found

```bash
python -c "import sounddevice as sd; print(sd.query_devices())"
```

Check your microphone is connected and select the correct device in `config/settings.yaml`.

### Model Download Fails

Check internet connection. Model downloads from Hugging Face on first run.

---

## Configuration

Edit `config/settings.yaml`:

```yaml
transcription:
  model: google/medasr
  device: cuda  # or cpu

audio:
  sample_rate: 16000
  channels: 1

hotkeys:
  toggle: ctrl+cmd  # ctrl+win
  cancel: escape
```

---

## System Requirements

- Windows 10/11
- NVIDIA GPU with CUDA support (RTX 3070 Ti or better)
- Python 3.9+
- ~2GB VRAM
- Microphone
