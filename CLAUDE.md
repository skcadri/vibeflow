# MedASR / VibeFlow - Voice Dictation App

## Overview

Windows desktop voice dictation application (Wispr Flow clone) that:
- Activates with **Ctrl+Win** keyboard shortcut
- Transcribes speech locally using Whisper models (default) or MedASR on RTX 3070 Ti
- Shows a floating bubble with waveform visualization during recording
- Pastes transcribed text at cursor position in any application (clipboard-based)
- Optional AI text formatting (capitalization, punctuation, lists, paragraphs)
- Stores transcript history in SQLite

## Quick Start

```bash
# Run from source
cd C:\code\medasr\src
python -m medasr

# Or use desktop shortcut
Double-click VibeFlow on Desktop
```

## Project Structure

```
C:\code\medasr\
├── VibeFlow.bat              # Launcher script
├── VibeFlow.vbs              # Windowless launcher wrapper
├── config/
│   ├── settings.yaml         # App configuration
│   └── vocabulary.txt        # Custom vocabulary/hotwords
├── src/medasr/
│   ├── __main__.py           # Entry point
│   ├── app.py                # Main controller + state machine
│   ├── config.py             # Settings management
│   ├── audio/
│   │   └── capture.py        # Microphone recording (sounddevice)
│   ├── transcription/
│   │   ├── medasr_model.py   # Google MedASR transcriber
│   │   └── whisper_variants.py # Whisper model variants (faster-whisper)
│   ├── input/
│   │   ├── hotkeys.py        # Global hotkey listener (pynput)
│   │   └── typer.py          # Clipboard paste at cursor
│   ├── postprocessing/
│   │   └── formatter.py      # Local LLM text formatter (Qwen2-0.5B)
│   ├── ui/
│   │   ├── bubble.py         # Floating bubble window (PyQt6)
│   │   ├── tray.py           # System tray icon (pystray)
│   │   ├── settings_window.py # Settings window with tabs
│   │   ├── vocabulary_tab.py # Vocabulary management
│   │   ├── models_tab.py     # Model selection
│   │   ├── formatting_tab.py # Text formatting toggle
│   │   ├── history_tab.py    # Transcription history
│   │   └── styles.py         # Light theme QSS styles
│   ├── vocabulary/
│   │   └── manager.py        # Hotwords manager
│   └── history/
│       └── storage.py        # SQLite history storage
└── data/
    └── history.db            # SQLite database (auto-created)
```

## Available Models

| Model Key | Description | Speed | VRAM |
|-----------|-------------|-------|------|
| whisper_tiny | Whisper tiny.en | Fastest | ~1GB |
| whisper_base | Whisper base.en (default) | Fast | ~1GB |
| whisper_small | Whisper small.en | Medium | ~2GB |
| whisper_medium | Whisper medium.en | Slow | ~5GB |
| whisper_large | Whisper large-v3 | Slowest | ~10GB |
| whisper_turbo | Whisper large-v3-turbo | Fast | ~6GB |
| distil_whisper | Distil-Whisper large-v3 | Fast | ~4GB |
| medasr | Google MedASR (medical) | Medium | ~1GB |

## Key Features

- **Model Switching**: Change models in settings, previous model is fully unloaded from VRAM
- **Custom Vocabulary**: Add medical terms or names as hotwords to improve recognition
- **Text Formatting**: Optional local LLM post-processing (see below)
- **Clipboard Paste**: Uses Ctrl+V to paste text (works with WhatsApp, etc.)
- **History**: All transcriptions saved with timestamp, model used, and duration
- **Settings Window**: Double-click tray icon or use Settings menu item

## Text Formatting (Optional)

Enable in Settings → Formatting tab to use a local AI model for text post-processing:

- **Model**: Qwen2-0.5B-Instruct (~0.5GB VRAM)
- **Features**: Capitalization, punctuation, bullet lists, paragraph breaks
- **Toggle**: Enable/disable anytime, model unloads when disabled to free VRAM

**Examples:**
- "i need milk eggs and bread" → bullet list
- "hello how are you" → "Hello, how are you?"
- "first point second point" → paragraphed text

**Note:** First enable downloads the model (~400MB). Disabled by default.

## Thread Safety

- pystray runs in separate thread from Qt main thread
- Uses `QCoreApplication.postEvent()` with custom `QEvent` for cross-thread communication
- All UI updates happen on Qt main thread via event handler

## GitHub Repository

https://github.com/skcadri/vibeflow.git
