# MedASR / VibeFlow - Voice Dictation App

## Overview

Windows desktop voice dictation application (Wispr Flow clone) that:
- Activates with **Ctrl+Win** keyboard shortcut
- Transcribes speech locally using Whisper models (default) or MedASR on RTX 3070 Ti
- Shows a floating bubble with waveform visualization during recording
- Pastes transcribed text at cursor position in any application (clipboard-based)
- Optional AI text formatting (capitalization, punctuation, lists, paragraphs)
- Supports CPU or CUDA acceleration for transcription
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
│   ├── config.py             # Settings management (has set/save methods)
│   ├── audio/
│   │   └── capture.py        # Microphone recording (sounddevice)
│   ├── transcription/
│   │   ├── medasr_model.py   # Google MedASR transcriber
│   │   └── whisper_variants.py # Whisper model variants (faster-whisper)
│   ├── input/
│   │   ├── hotkeys.py        # Global hotkey listener (pynput)
│   │   └── typer.py          # Clipboard paste at cursor (pyperclip + Ctrl+V)
│   ├── postprocessing/
│   │   └── formatter.py      # Local LLM text formatter (Phi-3-mini)
│   ├── ui/
│   │   ├── bubble.py         # Floating bubble window (PyQt6)
│   │   ├── tray.py           # System tray icon (pystray)
│   │   ├── settings_window.py # Settings window with tabs
│   │   ├── vocabulary_tab.py # Vocabulary management
│   │   ├── models_tab.py     # Model selection + CPU/CUDA device toggle
│   │   ├── formatting_tab.py # Text formatting toggle + editable prompts
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
- **CPU/CUDA Toggle**: Switch between GPU and CPU acceleration in Settings → Models tab. CPU uses int8 compute, CUDA uses float16. Device switch unloads and reinitializes the model.
- **Custom Vocabulary**: Add medical terms or names as hotwords to improve recognition
- **Text Formatting**: Optional local LLM post-processing (see below)
- **Clipboard Paste**: Uses Ctrl+V to paste text (works with WhatsApp, etc.)
- **History**: All transcriptions saved with timestamp, model used, and duration
- **Settings Window**: Double-click tray icon or use Settings menu item

## Text Formatting (Optional)

Enable in Settings → Formatting tab to use a local AI model for text post-processing:

- **Model**: Phi-3-mini-4k-instruct (~2.5GB VRAM) via llama-cpp-python (GGUF)
- **Features**: Paragraph breaks, bullet lists - preserves original words exactly
- **Fix Typos**: Optional setting to also fix obvious spelling mistakes
- **Editable Prompts**: Both strict and typo-fix prompts can be edited in the settings UI. The `{text}` placeholder is where transcribed text gets inserted. Prompts use Phi-3 chat format (`<|user|>...<|end|>\n<|assistant|>`).
- **Toggle**: Enable/disable anytime, model unloads when disabled to free VRAM
- **Default prompts**: Defined as `DEFAULT_PROMPT_STRICT` and `DEFAULT_PROMPT_TYPOFIX` in `formatter.py`. Custom prompts saved to config as `formatting.prompt_strict` / `formatting.prompt_typofix`.

**Note:** First enable downloads the model (~2GB). Disabled by default.

## Architecture Notes

### Config System (`config.py`)
- `config.get('dotted.key', default)` - Read nested YAML values
- `config.set('dotted.key', value)` - Set nested YAML values
- `config.save()` - Persist to `config/settings.yaml`

### App Controller (`app.py`)
- State machine: IDLE → RECORDING → PROCESSING → IDLE
- `switch_model(model_key)` - Unloads old model, loads new one async
- `switch_device(device)` - Changes CPU/CUDA, saves config, reinitializes model
- Formatter integration: reads `formatting.enabled` and `formatting.fix_typos` from config

### Settings Window Signal Flow
- `models_tab.model_selected` → `settings_window.model_changed` → `app.switch_model()`
- `models_tab.device_changed` → `settings_window.device_changed` → `app.switch_device()`
- `formatting_tab.formatting_toggled` → loads/unloads formatter model
- `formatting_tab` prompt editor → saves directly to config on every change

### Thread Safety
- pystray runs in separate thread from Qt main thread
- Uses `QCoreApplication.postEvent()` with custom `QEvent` for cross-thread communication
- All UI updates happen on Qt main thread via event handler
- Formatter uses `threading.RLock` for model load/unload/inference

## GitHub Repository

https://github.com/skcadri/vibeflow.git
