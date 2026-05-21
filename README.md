# VibeFlow

Native macOS voice dictation powered by [whisper.cpp](https://github.com/ggml-org/whisper.cpp). Hold **Cmd+Ctrl** to record, release to transcribe and paste at the cursor. Built with C++/Qt6 and Metal GPU acceleration on Apple Silicon.

Inspired by [Wispr Flow](https://wisprflow.com).

## Features

- **Hold-to-dictate**: Hold Cmd+Ctrl to record, release to transcribe and paste
- **GPU-accelerated**: whisper.cpp large-v3-turbo on Metal — same 99-language accuracy as large-v3, ~8× faster decode, half the disk (1.6 GB vs 3 GB)
- **Multilingual**: Automatic language detection (Hindi audio is forced to Urdu transcription via a built-in suppression pass)
- **Two insertion modes**:
  - **Paste mode** — copies text to clipboard, simulates Cmd+V (default; works everywhere)
  - **Type at Cursor** — Accessibility API text insertion with a paste fallback for apps like Terminal.app
- **Translate to English** — toggle to transcribe any source language directly into English
- **Recent Transcriptions** — browseable history of recent dictations
- **Custom Vocabulary** — user-supplied terms (medical jargon, names, etc.) injected as prompt context to bias whisper's decoding
- **Keep Microphone Active** — keep the mic warm to skip the ~2 s Core Audio wake-up delay (helpful with webcam mics)
- **HTTP transcription server** — optional embedded server on `127.0.0.1:8080` for programmatic use
- **Frosted glass UI**: Floating waveform bubble with liquid glass effect
- **System-wide**: Works in any app — TextEdit, VS Code, Safari, Notes, terminals, etc.
- **Escape to cancel**: Press Escape while recording to abort
- **Menu bar app**: Lives in the system tray, no Dock icon

## Demo

Hold Cmd+Ctrl → frosted glass bubble appears at the bottom of the screen with an animated waveform → speak → release → text appears at the cursor.

## Requirements

- macOS 14+ (Sonoma or later) — tested on macOS Tahoe (26.x)
- Apple Silicon (M1/M2/M3/M4)
- ~1.6 GB disk space for the whisper large-v3-turbo model
- Homebrew (for `qt@6` and `cmake`)

## Install on Your Mac

VibeFlow ships as source code that you build locally. The block below is **fully self-contained** — it installs Homebrew if you don't have it, fetches every dependency, downloads the whisper model, builds the app, and installs it to `/Applications`. It takes 10–25 minutes depending on your internet speed.

You don't need to be a developer. Just open **Terminal.app** (press Cmd+Space → type "Terminal" → Enter), paste the whole block below, and press Return:

```bash
set -e

# 1. Sanity checks: Apple Silicon + macOS 14+
if [[ "$(uname -m)" != "arm64" ]]; then
  echo "❌ VibeFlow requires Apple Silicon (M1/M2/M3/M4). Detected: $(uname -m)"; exit 1
fi
if [[ "$(sw_vers -productVersion | cut -d. -f1)" -lt 14 ]]; then
  echo "❌ VibeFlow requires macOS 14 (Sonoma) or later. Detected: $(sw_vers -productVersion)"; exit 1
fi

# 2. Install Homebrew if missing (will prompt for sudo password the first time)
if ! command -v brew &>/dev/null; then
  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
  eval "$(/opt/homebrew/bin/brew shellenv)"
fi

# 3. Build dependencies
brew install qt@6 cmake git

# 4. Clone the source (skips if already present)
SRC=~/vibeflow-src
if [ ! -d "$SRC" ]; then
  git clone --recurse-submodules https://github.com/skcadri/vibeflow.git "$SRC"
fi
cd "$SRC"

# 5. Download the whisper large-v3-turbo model (~1.6 GB, one-time)
bash scripts/download-model.sh

# 6. Build, bundle, and code-sign
bash scripts/build.sh

# 7. Install to /Applications
osascript -e 'tell application "VibeFlow" to quit' 2>/dev/null || true
pkill -x VibeFlow 2>/dev/null || true
rm -rf /Applications/VibeFlow.app
cp -R build/VibeFlow.app /Applications/

# 8. Launch
open /Applications/VibeFlow.app

cat <<'BANNER'

================================================================
  ✅ VibeFlow is installed and launching now.

  Two permissions to grant on first launch:

  1) MICROPHONE — macOS will pop up a dialog. Click "Allow".

  2) ACCESSIBILITY — required for VibeFlow to type at your cursor.
     Open System Settings → Privacy & Security → Accessibility,
     find "VibeFlow" in the list, and toggle it ON.

     Shortcut: paste this in Terminal to jump straight there:
     open "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"

  Then hold ⌘+Ctrl, speak, release — text appears at your cursor.
================================================================
BANNER
```

### Give this to your AI agent

If you'd rather have a coding agent (Claude Code, Cursor, ChatGPT with shell access, etc.) do the install for you, paste this prompt:

> Set up VibeFlow on this Mac for me. The repo is https://github.com/skcadri/vibeflow and the canonical install instructions are in its README under "Install on Your Mac". Verify the machine is Apple Silicon and macOS 14+, install Homebrew if missing, install `qt@6` and `cmake` via Homebrew, clone the repo to `~/vibeflow-src` with `--recurse-submodules`, run `bash scripts/download-model.sh` to fetch the ~1.6 GB whisper large-v3-turbo model, run `bash scripts/build.sh` to build, then quit any running VibeFlow and install with `rm -rf /Applications/VibeFlow.app && cp -R build/VibeFlow.app /Applications/`. After install, walk me through granting Microphone (auto-prompt) and Accessibility (System Settings → Privacy & Security → Accessibility → toggle VibeFlow on) permissions. If anything fails, show me the error and suggest a fix.

### First Launch

macOS will prompt for permissions on first launch:
1. **Microphone** — click "Allow" (required for recording)
2. **Accessibility** — System Settings → Privacy & Security → Accessibility → enable VibeFlow (required for keystroke simulation / paste)

### After Rebuilds — Reset TCC Permissions

macOS revokes Accessibility and Microphone permissions when the code signature hash changes, which happens on every rebuild. **After reinstalling**, run:

```bash
osascript -e 'tell application "VibeFlow" to quit' 2>/dev/null
pkill -x VibeFlow 2>/dev/null
tccutil reset Accessibility com.sohaib.vibeflow
tccutil reset Microphone   com.sohaib.vibeflow
tccutil reset ListenEvent  com.sohaib.vibeflow
tccutil reset PostEvent    com.sohaib.vibeflow
```

Then re-launch and grant permissions again. Without this, you'll see silence from the mic or "Type at Cursor failed" tray messages with no obvious cause. `CLAUDE.md` has the full background.

## Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                            App.cpp                                │
│                    (state machine controller)                     │
│              Idle ←→ Recording ←→ Processing                      │
├──────────┬──────────┬───────────────┬──────────┬─────────────────┤
│ Hotkey   │ Audio    │ Whisper       │ HTTP     │ UI               │
│ Monitor  │ Capture  │ Transcriber   │ Server   │                  │
│          │          │               │ (opt-in) │                  │
│ CGEvent  │ QAudio   │ whisper.cpp   │ QTcpSrv  │ ┌─────────────┐  │
│ Flags    │ Source   │ (Metal GPU)   │ on 8080  │ │ GlassBubble │  │
│ polling  │ pull-mode│ + vocab prompt│          │ │  Waveform   │  │
│ @60Hz    │ Int16Mono│ + Hindi→Urdu  │          │ │  TrayIcon   │  │
│          │ 16kHz    │   suppression │          │ │  Dialogs    │  │
└──────────┴──────────┴───────────────┴──────────┴─────────────────┘
            macOS APIs                            Qt6 Widgets
       (CoreGraphics, AppKit, AX)
                                                         │
                                   ┌─────────────────────┴─────┐
                                   │ SettingsManager (QSettings)│
                                   │ - Recent transcriptions   │
                                   │ - Custom vocabulary       │
                                   │ - Mode toggles            │
                                   └───────────────────────────┘
```

## Project Structure

```
vibeflow/
├── CLAUDE.md                       # Project guide for AI agents (read first)
├── AGENTS.md                       # Detailed codebase guide for contributors
├── CMakeLists.txt                  # Build configuration
├── src/
│   ├── main.cpp                    # Entry point
│   ├── App.h / App.cpp             # State machine controller
│   ├── Transcriber.h / .cpp        # whisper.cpp wrapper (model load + transcribe)
│   ├── TranscriptionServer.h/.cpp  # Optional HTTP server on 127.0.0.1:8080
│   ├── AudioCapture.h / .cpp       # Mic recording (pull-mode QAudioSource)
│   ├── HotkeyMonitor.h / .mm       # Cmd+Ctrl detection (CGEventSourceFlagsState polling)
│   ├── TextPaster.h / .mm          # Paste + Type-at-Cursor (AX text insertion + Cmd+V fallback)
│   ├── data/
│   │   └── SettingsManager.h/.cpp  # QSettings-backed vocab + history + toggle persistence
│   └── ui/
│       ├── GlassBubble.h / .mm     # Frosted glass floating pill
│       ├── WaveformWidget.h/.cpp   # Animated 24-bar equalizer
│       ├── TrayIcon.h / .cpp       # Menu bar icon + tray menu
│       ├── RecentTranscriptionsDialog.h/.cpp  # History browser
│       └── VocabularyDialog.h/.cpp # Custom-term editor
├── deps/
│   ├── whisper.cpp/                # Git submodule (v1.8.3+)
│   └── qt-liquid-glass/            # Git submodule
├── resources/
│   └── Info.plist                  # macOS bundle metadata
├── scripts/
│   ├── build.sh                    # Build + bundle + sign + model-copy pipeline
│   └── download-model.sh           # Model download helper (Hugging Face)
└── models/                         # Model files (gitignored)
    └── ggml-large-v3-turbo.bin     # 1.6 GB
```

## Tray Menu

Right-click the menu bar icon to access:

- **Type at Cursor** — toggle insertion mode (paste vs AX-typing)
- **Translate to English** — toggle direct-to-English transcription
- **Keep Microphone Active** — toggle mic warm-keep
- **Enable Transcription Server** — toggle the HTTP server on `127.0.0.1:8080`
- **Recent Transcriptions…** — browse history
- **Vocabulary…** — edit custom prompt vocabulary
- **Test Paste** — sanity-check Accessibility permissions
- **About VibeFlow** / **Quit**

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
1. CMake configure + incremental build
2. `macdeployqt` to bundle Qt frameworks
3. `install_name_tool` to fix Homebrew rpath references
4. `codesign` — prefers the stable `"VibeFlow Dev"` identity if present in keychain, falls back to ad-hoc (`-`). Stable signing helps macOS persist TCC permissions across rebuilds. Override with `CODESIGN_IDENTITY=…` env var.
5. Copy the whisper model into `Contents/Resources/` (if present in `models/`)

## Troubleshooting

- **Mic returns silence / "Type at Cursor failed"** — see the [TCC reset workflow](#after-rebuilds--reset-tcc-permissions) above. Almost always a code-signature-changed-after-rebuild issue.
- **Diagnostic logs** — run from terminal to see the `fprintf(stderr, ...)` traces:
  ```bash
  /Applications/VibeFlow.app/Contents/MacOS/VibeFlow 2>&1 | tee /tmp/vf.log
  ```
- **Hindi/Urdu transcription quality** — Whisper sometimes auto-detects Hindi for Urdu audio. VibeFlow re-runs transcription with Urdu forced when this happens; see `src/Transcriber.cpp`.
- **More detail** — `AGENTS.md` has the full file-by-file reference and historical bug diagnoses.

## License

MIT
