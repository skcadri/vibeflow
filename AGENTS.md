# AGENTS.md — VibeFlow Codebase Guide

This file helps AI coding agents (and human contributors) navigate and understand the VibeFlow codebase quickly.

## What is VibeFlow?

A native macOS voice dictation app. Hold **Cmd+Ctrl** to record, release to transcribe with whisper.cpp (large-v3 model on Metal GPU), and paste the text at the cursor position. A frosted glass waveform bubble appears at the bottom of the screen during recording.

Think of it as an open-source alternative to [Wispr Flow](https://wisprflow.com).

## Tech Stack

| Component | Technology |
|-----------|------------|
| Language | C++17 / Objective-C++ |
| UI framework | Qt 6 (Widgets + Multimedia) |
| Glass effect | [qt-liquid-glass](https://github.com/fsalinas26/qt-liquid-glass) |
| Transcription | [whisper.cpp](https://github.com/ggml-org/whisper.cpp) with Metal GPU |
| Audio capture | Qt Multimedia (`QAudioSource`) |
| Global hotkey | `CGEventSourceFlagsState` polling (60Hz) |
| Paste at cursor | `NSPasteboard` + `CGEventPost` (Cmd+V simulation) |
| Build system | CMake 3.21+ |

## Architecture

### State Machine

The app is a simple 3-state machine managed by `App.cpp`:

```
         Cmd+Ctrl held              Cmd+Ctrl released
[Idle] ──────────────> [Recording] ──────────────────> [Processing] ──> [Idle]
                          │                                │
                     Escape pressed                   Text pasted
                          │                           at cursor
                          v
                        [Idle]
```

### Signal Flow

```
HotkeyMonitor::activated()
  → App::onHotkeyActivated()
    → AudioCapture::start()
    → GlassBubble::setState(Recording)

AudioCapture::levelChanged(float rms)
  → GlassBubble::updateLevel()
    → WaveformWidget::updateLevel()

HotkeyMonitor::deactivated()
  → App::onHotkeyDeactivated()
    → AudioCapture::stop()
    → GlassBubble::setState(Processing)
    → Transcriber::transcribe() [runs in QThread]

Transcriber completes
  → App::onTranscriptionFinished(QString text)
    → TextPaster::paste(text)
    → GlassBubble::setState(Hidden)
```

### Component Ownership

```
main.cpp
  └─ App (QObject, stack-allocated)
       ├─ Transcriber (QObject child)
       ├─ AudioCapture (QObject child)
       │    └─ AudioBuffer (custom QIODevice for pull-mode recording)
       ├─ HotkeyMonitor (QObject child)
       │    └─ QTimer (60Hz modifier key polling)
       ├─ GlassBubble (QWidget, top-level window)
       │    └─ WaveformWidget (child widget)
       └─ TrayIcon (QObject child)
            └─ QSystemTrayIcon
```

## File Map

### Core

| File | Purpose | Key Details |
|------|---------|-------------|
| `src/main.cpp` | Entry point | Creates QApplication, installs stderr message handler, enters event loop |
| `src/App.h/.cpp` | State machine controller | Wires all components together, manages Idle/Recording/Processing states |
| `CMakeLists.txt` | Build configuration | Qt6, whisper.cpp (Metal), qt-liquid-glass, ObjC++ support |

### Audio Pipeline

| File | Purpose | Key Details |
|------|---------|-------------|
| `src/AudioCapture.h/.cpp` | Microphone recording | **Pull-mode** `QAudioSource` → custom `AudioBuffer` QIODevice. Handles format negotiation (prefers 16kHz mono Int16, falls back to device preferred format). Resamples to 16kHz mono float32 for whisper. Emits RMS levels at 30fps for waveform |
| `src/Transcriber.h/.cpp` | whisper.cpp wrapper | Loads model async in QThread. Transcribes float32 16kHz mono audio. Uses `WHISPER_SAMPLING_GREEDY`, `language="auto"`, 8 threads. Metal GPU enabled |
| `src/TextPaster.h/.mm` | Clipboard + paste | Sets `NSPasteboard`, waits 50ms, simulates Cmd+V via `CGEventPost` |

### Input

| File | Purpose | Key Details |
|------|---------|-------------|
| `src/HotkeyMonitor.h/.mm` | Global Cmd+Ctrl detection | **Polls** `CGEventSourceFlagsState` at 60Hz via QTimer. No CGEventTap needed (avoids Input Monitoring permission issues on macOS Tahoe). Also polls `CGEventSourceKeyState` for Escape cancellation |

### UI

| File | Purpose | Key Details |
|------|---------|-------------|
| `src/ui/GlassBubble.h/.mm` | Frosted glass floating pill | 300×56px, frameless, `NSScreenSaverWindowLevel` (always on top). Uses `QtLiquidGlass::Material::Hud`. Positioned above Dock via `availableGeometry()`. Fade in/out via `QPropertyAnimation` on window opacity |
| `src/ui/WaveformWidget.h/.cpp` | 24-bar animated equalizer | Bars: 4px wide, 2px gap, white 80% opacity. Height: 4–40px, smooth LERP (factor 0.3). 30fps QTimer animation. Idle bars at ~4px, peaks at 40px |
| `src/ui/TrayIcon.h/.cpp` | Menu bar icon | Programmatic mic glyph (QPainterPath), set as NSImage template. Menu: "About VibeFlow" + "Quit" |

### Build & Config

| File | Purpose |
|------|---------|
| `scripts/build.sh` | Full pipeline: cmake → build → macdeployqt → rpath fix → codesign → model copy |
| `scripts/download-model.sh` | Downloads ggml-large-v3.bin (~3GB) from Hugging Face |
| `resources/Info.plist` | Bundle metadata. `LSUIElement=true` (no Dock icon). `NSMicrophoneUsageDescription` for TCC prompt |
| `.gitmodules` | Submodules: `deps/whisper.cpp`, `deps/qt-liquid-glass` |

## Known Bug: Audio Capture Returns Silence

**Status**: Unresolved. This is the primary blocker.

**Symptom**: `QAudioSource` reports `ActiveState` with `NoError`, the Yeti Stereo Microphone is detected, 16kHz mono Int16 is listed as natively supported, but **zero bytes flow through the pull-mode AudioBuffer** (or in some builds, bytes flow but contain silence with rms ≈ 0.0005).

**Root cause (suspected)**: macOS Tahoe (26.x) TCC microphone permission is not persisting across app launches. Because the app is ad-hoc signed (`codesign --sign -`), each rebuild produces a new code signature hash, and macOS revokes the mic permission. The OS then delivers silence instead of failing the API — `QAudioSource` has no way to know permission was denied.

**Evidence**:
- macOS prompts for microphone permission on every launch (should only prompt once)
- The mic works in other apps (not a hardware issue)
- `QAudioSource` state = Active, error = NoError, but data = zeros
- The Yeti device is correctly identified and format is supported

**What to try**:
1. **System Settings → Privacy & Security → Microphone** — verify VibeFlow is toggled ON (not just Accessibility)
2. `tccutil reset Microphone com.sohaib.vibeflow` then relaunch and click "Allow"
3. Sign with a stable developer certificate instead of ad-hoc (`codesign --sign "Developer ID"`) so TCC remembers the permission
4. As a workaround, try using **AVFoundation** (`AVAudioEngine` / `AVAudioSession`) instead of Qt Multimedia for audio capture on macOS — Apple's own framework handles TCC permissions more reliably
5. Try building and running from **Xcode** (not command-line) to get automatic provisioning

**Diagnostic logging**: All components emit `fprintf(stderr, ...)` logs. Run the app from terminal to see output:
```bash
/Applications/VibeFlow.app/Contents/MacOS/VibeFlow 2>&1 | tee /tmp/vf.log
```

## Build Instructions

### Prerequisites

```bash
brew install qt@6 cmake
```

### Setup

```bash
git clone --recurse-submodules https://github.com/skcadri/vibeflow.git
cd vibeflow

# Download whisper large-v3 model (~3GB)
bash scripts/download-model.sh
```

### Build & Install

```bash
bash scripts/build.sh
rm -rf /Applications/VibeFlow.app && cp -R build/VibeFlow.app /Applications/
```

Or manually:
```bash
cmake -B build -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)
```

### Run from dev build (no install)

```bash
./build/VibeFlow.app/Contents/MacOS/VibeFlow
```

### macOS Permissions Required

1. **Microphone** — System Settings → Privacy & Security → Microphone → enable VibeFlow
2. **Accessibility** — System Settings → Privacy & Security → Accessibility → enable VibeFlow (for hotkey detection fallback)

## CMake Build Details

- `WHISPER_METAL ON` — GPU acceleration on Apple Silicon
- `WHISPER_METAL_EMBED_LIBRARY ON` — Metal shader embedded in binary (no external .metallib)
- `WHISPER_COREML OFF` — CoreML has build compatibility issues on macOS Tahoe
- `.mm` files compiled with `-fobjc-arc` (automatic reference counting)
- `ggml-metal.m` forced to compile as `LANGUAGE OBJC` (CMake would default to plain C)

## Coding Conventions

- All logging via `fprintf(stderr, "[LEVEL] Component: message\n")` + `fflush(stderr)`
- Qt signals/slots for inter-component communication
- Background work in `QThread::create()` lambdas with `QMetaObject::invokeMethod` for results
- Objective-C++ (`.mm`) files for macOS-specific APIs (AppKit, CoreGraphics)
- No exceptions, no RTTI — standard Qt/C++ patterns
