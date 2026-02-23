# VibeFlow

Native macOS voice dictation powered by [whisper.cpp](https://github.com/ggml-org/whisper.cpp). Hold **Cmd+Ctrl** to record, release to transcribe and paste at cursor. Built with C++/Qt6 and Metal GPU acceleration on Apple Silicon.

Inspired by [Wispr Flow](https://wisprflow.com).

## Features

- **Hold-to-dictate**: Hold Cmd+Ctrl to record, release to transcribe and paste
- **GPU-accelerated**: whisper.cpp large-v3 model running on Metal (Apple Silicon)
- **Multilingual**: Automatic language detection — English, Urdu, and 98 other languages
- **Frosted glass UI**: Floating waveform bubble with liquid glass effect
- **System-wide**: Works in any app — dictate into TextEdit, VS Code, Safari, Notes, etc.
- **Escape to cancel**: Press Escape while recording to abort
- **Menu bar app**: Lives in the system tray, no Dock icon

## Demo

Hold Cmd+Ctrl → frosted glass bubble appears at bottom of screen with animated waveform → speak → release → text appears at cursor.

## Requirements

- macOS 14+ (Sonoma or later)
- Apple Silicon (M1/M2/M3/M4)
- ~3GB disk space for the whisper large-v3 model
- Homebrew

## Quick Start

```bash
# Install dependencies
brew install qt@6 cmake

# Clone with submodules
git clone --recurse-submodules https://github.com/skcadri/vibeflow.git
cd vibeflow

# Download the whisper large-v3 model (~3GB)
bash scripts/download-model.sh

# Build, bundle, and sign
bash scripts/build.sh

# Install
rm -rf /Applications/VibeFlow.app && cp -R build/VibeFlow.app /Applications/
```

Launch from Applications or:
```bash
open /Applications/VibeFlow.app
```

### First Launch

macOS will prompt for two permissions:
1. **Microphone** — click "Allow" (required for recording)
2. **Accessibility** — System Settings → Privacy & Security → Accessibility → enable VibeFlow

## Architecture

```
┌────────────────────────────────────────────────────┐
│                     App.cpp                         │
│              (state machine controller)             │
│         Idle ←→ Recording ←→ Processing             │
├──────────┬──────────┬──────────┬───────────────────┤
│ Hotkey   │ Audio    │ Whisper  │ UI                 │
│ Monitor  │ Capture  │ Transcr. │                    │
│          │          │          │ ┌───────────────┐  │
│ CGEvent  │ QAudio   │ whisper  │ │ GlassBubble   │  │
│ Source   │ Source   │ .cpp     │ │ ┌───────────┐ │  │
│ Flags    │ (pull    │ (Metal   │ │ │ Waveform  │ │  │
│ State    │  mode)   │  GPU)    │ │ │ Widget    │ │  │
│ polling  │          │          │ │ └───────────┘ │  │
│ @60Hz    │          │          │ └───────────────┘  │
│          │          │          │ TrayIcon            │
└──────────┴──────────┴──────────┴───────────────────┘
         macOS APIs              Qt6 Widgets
    (CoreGraphics, AppKit)
```

## Project Structure

```
vibeflow/
├── CMakeLists.txt              # Build configuration
├── src/
│   ├── main.cpp                # Entry point
│   ├── App.h / App.cpp         # State machine controller
│   ├── Transcriber.h / .cpp    # whisper.cpp wrapper
│   ├── AudioCapture.h / .cpp   # Mic recording (pull-mode QAudioSource)
│   ├── HotkeyMonitor.h / .mm   # Cmd+Ctrl detection (CGEventSourceFlagsState polling)
│   ├── TextPaster.h / .mm      # Clipboard + Cmd+V paste simulation
│   └── ui/
│       ├── GlassBubble.h / .mm # Frosted glass floating pill
│       ├── WaveformWidget.h/.cpp# Animated 24-bar equalizer
│       └── TrayIcon.h / .cpp   # Menu bar icon
├── deps/
│   ├── whisper.cpp/            # Git submodule
│   └── qt-liquid-glass/        # Git submodule
├── resources/
│   └── Info.plist              # macOS bundle metadata
├── scripts/
│   ├── build.sh                # Build + bundle + sign pipeline
│   └── download-model.sh       # Model download helper
├── models/                     # Model files (gitignored)
│   └── ggml-large-v3.bin
└── AGENTS.md                   # Detailed codebase guide for contributors
```

## Current Status

**Working**:
- Hotkey detection (Cmd+Ctrl hold/release via polling)
- Whisper model loading with Metal GPU acceleration (3GB large-v3)
- Frosted glass bubble UI with waveform animation
- State machine (Idle → Recording → Processing → Idle)
- Menu bar tray icon
- Text paste at cursor simulation
- macOS app bundle with bundled Qt frameworks

**Not working**:
- **Audio capture delivers silence** — `QAudioSource` reports active with no errors, but macOS TCC microphone permission doesn't persist across ad-hoc signed rebuilds. See [AGENTS.md](AGENTS.md) for detailed diagnosis and suggested fixes.

## Building from Source

### Manual Build

```bash
cmake -B build \
    -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build -j$(sysctl -n hw.ncpu)
```

The resulting app bundle is at `build/VibeFlow.app`.

### Build Script

`scripts/build.sh` handles the full pipeline:
1. CMake configure + build
2. `macdeployqt` to bundle Qt frameworks
3. `install_name_tool` to fix Homebrew rpath references
4. `codesign` to re-sign everything (ad-hoc)
5. Copy whisper model into app bundle Resources

## License

MIT
