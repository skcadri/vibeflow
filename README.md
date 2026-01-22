# MedASR Desktop - Voice Dictation App

A Windows desktop voice dictation application inspired by Wispr Flow, using Google's MedASR model for local GPU-accelerated transcription.

## Features

- **Global Hotkey**: Press `Ctrl+Win` to start/stop dictation
- **Local GPU Transcription**: Uses Google MedASR on your NVIDIA GPU
- **Floating Bubble UI**: Visual waveform feedback during recording
- **Universal**: Types text at cursor in any application
- **History**: Stores transcripts with SQLite

## Installation

1. **Request MedASR Access**:
   - Create account at https://huggingface.co/join
   - Request access at https://huggingface.co/google/medasr
   - Get your token from https://huggingface.co/settings/tokens

2. **Install CUDA PyTorch**:
```bash
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu121
```

3. **Install dependencies**:
```bash
pip install -r requirements.txt
```

4. **Authenticate**:
   - Your token is already saved in `.env` file
   - Or set environment variable: `$env:HF_TOKEN = "your_token"`

5. **Run the app**:
```bash
python -m src.medasr
```

## Usage

1. Launch the app
2. Position your cursor in any text field
3. Press and hold `Ctrl+Win` to start recording
4. Release `Ctrl+Win` to transcribe and type
5. Press `Escape` while recording to cancel

## Requirements

- Windows 10/11
- NVIDIA GPU with CUDA support (tested on RTX 3070 Ti)
- Python 3.9+
- ~2GB VRAM for MedASR model

## Configuration

Edit `config/settings.yaml` to customize hotkeys, audio settings, and UI behavior.

## License

GPL-3.0 (inspired by whisper-writer)
