# AGENTS.md — VibeFlow Codebase Guide

This file helps AI coding agents (and human contributors) navigate and understand the VibeFlow codebase quickly.

## What is VibeFlow?

A native macOS voice dictation app. Hold **Cmd+Ctrl** to record, release to transcribe with transcribe.cpp (Qwen3-ASR 1.7B on Metal GPU, Whisper large-v3-turbo for Urdu), and paste the text at the cursor position. A frosted glass waveform bubble appears at the bottom of the screen during recording, with an optional live-text panel above it (Nemotron streaming model) showing words as you speak.

Think of it as an open-source alternative to [Wispr Flow](https://wisprflow.com).

## Tech Stack

| Component | Technology |
|-----------|------------|
| Language | C++17 / Objective-C++ |
| UI framework | Qt 6 (Widgets + Multimedia) |
| Glass effect | [qt-liquid-glass](https://github.com/fsalinas26/qt-liquid-glass) |
| Transcription | [transcribe.cpp](https://github.com/handy-computer/transcribe.cpp) (ggml, Metal GPU): Qwen3-ASR 1.7B + Whisper large-v3-turbo; Nemotron Speech Streaming 0.6B for live preview |
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
    → LivePreview::start()          [if "Show Live Text" is on]
    → AudioCapture::start()
    → GlassBubble::setState(Recording)

AudioCapture::levelChanged(float rms)
  → GlassBubble::updateLevel()
    → WaveformWidget::updateLevel()

AudioCapture::audioChunk(16 kHz samples, ~30/s)
  → LivePreview::feed()             [own QThread, Nemotron stream]
    → LivePreview::textChanged(committed, tentative)
      → LiveTextBubble::setText()

HotkeyMonitor::deactivated()
  → App::onHotkeyDeactivated()
    → AudioCapture::stop()
    → LivePreview::stop()           [live text stays up while Qwen runs]
    → GlassBubble::setState(Processing)
    → Transcriber::transcribe() [runs in QThread]

Transcriber completes
  → App::onTranscriptionFinished(QString text)
    → GlassBubble::hideImmediately()
    → LiveTextBubble::hideNow()        [never linger after release: it sits over what you click next]
    → TextPaster::paste(text)
```

### Component Ownership

```
main.cpp
  └─ App (QObject, stack-allocated)
       ├─ Transcriber (QObject child)
       ├─ LivePreview (no parent; moved to its own QThread, deleted when it finishes)
       ├─ AudioCapture (QObject child)
       │    └─ AudioBuffer (custom QIODevice for pull-mode recording)
       ├─ HotkeyMonitor (QObject child)
       │    └─ QTimer (60Hz modifier key polling)
       ├─ GlassBubble (QWidget, top-level window)
       │    └─ WaveformWidget (child widget)
       ├─ LiveTextBubble (QWidget, top-level window anchored above GlassBubble)
       └─ TrayIcon (QObject child)
            └─ QSystemTrayIcon
```

## File Map

### Core

| File | Purpose | Key Details |
|------|---------|-------------|
| `src/main.cpp` | Entry point | Creates QApplication, installs stderr message handler, enters event loop |
| `src/App.h/.cpp` | State machine controller | Wires all components together, manages Idle/Recording/Processing states |
| `CMakeLists.txt` | Build configuration | Qt6, transcribe.cpp (Metal), qt-liquid-glass, ObjC++ support |

### Audio Pipeline

| File | Purpose | Key Details |
|------|---------|-------------|
| `src/AudioCapture.h/.cpp` | Microphone recording | **Pull-mode** `QAudioSource` → custom `AudioBuffer` QIODevice. Handles format negotiation (prefers 16kHz mono Int16, falls back to device preferred format). Resamples to 16kHz mono float32 for the transcriber (`convertTo16kMono`, shared by the final pass and the live chunks). Emits RMS levels at 30fps for waveform and `audioChunk` (just the new samples) at the same rate for live preview |
| `src/Transcriber.h/.cpp` | transcribe.cpp wrapper | Loads both models async in a QThread. `transcribe()` (hotkey) runs **Qwen3-ASR**; if the output contains any Devanagari (Qwen has no Urdu, so Urdu comes back as Hindi script) it re-runs **Whisper** with `language="ur"`. `transcribeWithSegments()` (HTTP server) runs **Whisper** because Qwen3-ASR has no timestamps. Vocabulary goes through transcribe.cpp's `vocabulary` field, which formats it per model — never hand-build a prompt string from it. Every segment is cleaned via `isHallucination()` and `stripSpeakerLabel()` (see note below) |
| `src/LivePreview.h/.cpp` | Live text while speaking | Separate **Nemotron Speech Streaming 0.6B** session (English-only, `transcribe_stream_*` API, lookahead 6 = 480 ms) on its own QThread. **Preview only** — never paste its text; Qwen's result is what gets pasted. Its model is optional: missing file = no live text, everything else unchanged. Qwen3-ASR can't stream (transcribe.cpp marks it unsupported), so don't try to drive the preview from `Transcriber` |
| `src/TextPaster.h/.mm` | Clipboard + paste | Sets `NSPasteboard`, waits 50ms, simulates Cmd+V via `CGEventPost` |

**Speaker-label hallucination note:** Whisper invents fake speaker labels (e.g. `Tim Cynova:`, `Artyaz, Mathias, Salman, Oswego, :`) at the start of any segment. The trigger was a vocabulary prompt shaped like `Vocabulary: name, name, …` — Whisper copies the prompt's shape. transcribe.cpp independently dropped its `Glossary: …` vocabulary header for the same reason (upstream commit `8facde6`). Rules:
- **Never wrap vocabulary in a `Label: list` header.** Pass the raw word list to `transcribe_run_params::vocabulary`.
- **Strip per segment.** `Transcriber::stripSpeakerLabel()` removes a leading run of 1–6 capitalized words (optionally comma-separated) followed by a colon. `cleanSegments()` applies it to every segment before joining. Spoken dictation never contains a literal colon, so this is safe (tradeoff: a deliberately dictated leading `Note:`/`Action Items:` is also stripped).
- `TranscriptionServer::transcribeChunked()` also strips the chunk **carry-forward** prompt, so a label can't be re-injected into every later 30s chunk.

### Input

| File | Purpose | Key Details |
|------|---------|-------------|
| `src/HotkeyMonitor.h/.mm` | Global Cmd+Ctrl detection | **Polls** `CGEventSourceFlagsState` at 60Hz via QTimer. No CGEventTap needed (avoids Input Monitoring permission issues on macOS Tahoe). Also polls `CGEventSourceKeyState` for Escape cancellation |

### UI

| File | Purpose | Key Details |
|------|---------|-------------|
| `src/ui/GlassBubble.h/.mm` | Frosted glass floating pill | 300×56px, frameless, `NSScreenSaverWindowLevel` (always on top). Uses `QtLiquidGlass::Material::Hud`. Positioned above Dock via `availableGeometry()`. Fade in/out via `QPropertyAnimation` on window opacity |
| `src/ui/WaveformWidget.h/.cpp` | 24-bar animated equalizer | Bars: 4px wide, 2px gap, white 80% opacity. Height: 4–40px, smooth LERP (factor 0.3). 30fps QTimer animation. Idle bars at ~4px, peaks at 40px |
| `src/ui/LiveTextBubble.h/.cpp` | Live-text glass panel | Sits above the pill; width 45% of the screen, grows upward with the text to a third of the screen height, then oldest lines clip off the top. Settled words bright, still-changing words dim. Look-and-feel knobs are the constants at the top of the file. Reuses `GlassBubble::showAboveEverything()` for window level/Spaces |
| `src/ui/TrayIcon.h/.cpp` | Menu bar icon | Programmatic mic glyph (QPainterPath), set as NSImage template. Toggles: Type at Cursor, Keep Microphone Active, **Show Live Text While Speaking** (persisted in `SettingsManager`, default on), Transcription Server |

### Build & Config

| File | Purpose |
|------|---------|
| `scripts/build.sh` | Full pipeline: cmake → build → macdeployqt → rpath fix → codesign → model copy |
| `scripts/download-model.sh` | Downloads the GGUF models (~3.8 GB) into `models/` |
| `resources/Info.plist` | Bundle metadata. `LSUIElement=true` (no Dock icon). `NSMicrophoneUsageDescription` for TCC prompt |
| `.gitmodules` | Submodules: `deps/transcribe.cpp` (pinned past v0.4.1 for the vocabulary-header fix), `deps/qt-liquid-glass`. `deps/httplib/httplib.h` is vendored (single header, MIT) for the HTTP server |

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

# Download Qwen3-ASR + Whisper + Nemotron (live text) GGUF models (~3.8GB)
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

- transcribe.cpp enables Metal automatically on Apple Silicon; `GGML_METAL_EMBED_LIBRARY ON` embeds the shaders (no external .metallib)
- transcribe.cpp sets its public include dir from `CMAKE_SOURCE_DIR`, which breaks under `add_subdirectory()`. `CMakeLists.txt` re-adds `deps/transcribe.cpp/include` to the `transcribe` target — keep that line when bumping the submodule until upstream fixes it
- `.mm` files compiled with `-fobjc-arc` (automatic reference counting)

## Coding Conventions

- All logging via `fprintf(stderr, "[LEVEL] Component: message\n")` + `fflush(stderr)`
- Qt signals/slots for inter-component communication
- Background work in `QThread::create()` lambdas with `QMetaObject::invokeMethod` for results
- Objective-C++ (`.mm`) files for macOS-specific APIs (AppKit, CoreGraphics)
- No exceptions, no RTTI — standard Qt/C++ patterns
