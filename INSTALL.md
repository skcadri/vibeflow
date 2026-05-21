# Installing VibeFlow

VibeFlow ships as source — you build it locally on your Mac. There are two ways to install:

1. [**Hand it to your AI agent**](#agent-install) (easiest — recommended for non-developers)
2. [**Do it yourself in Terminal**](#manual-install) (one copy-paste block)

Either way takes 10–25 minutes depending on your internet speed.

**Requirements:**
- macOS 14+ (Sonoma or later)
- Apple Silicon Mac (M1/M2/M3/M4) — Intel Macs are not supported
- ~5 GB free disk space (1.6 GB model + Qt6 + build artifacts)

---

## Agent install

Paste the prompt below into **Claude Code**, **Cursor**, **ChatGPT** (with shell access), or any coding agent that can run terminal commands on your Mac. The agent will do everything — install Homebrew, fetch dependencies, build the app, install it, and walk you through granting permissions.

```
Install VibeFlow on this Mac for me.

Follow the instructions at:
https://github.com/skcadri/vibeflow/blob/main/INSTALL.md

Specifically:
1. Verify Apple Silicon + macOS 14+; fail early if not.
2. Install Homebrew if missing.
3. brew install qt@6 cmake git
4. Clone https://github.com/skcadri/vibeflow.git with --recurse-submodules to ~/vibeflow-src
5. Run `bash scripts/download-model.sh` to fetch the 1.6 GB whisper model
6. Run `bash scripts/build.sh` to build and code-sign
7. Quit any running VibeFlow, then install with:
   rm -rf /Applications/VibeFlow.app && cp -R build/VibeFlow.app /Applications/
8. Launch the app and walk me through granting Microphone (auto-prompts) and Accessibility (System Settings → Privacy & Security → Accessibility → toggle VibeFlow on).

If anything fails, show me the error and propose a fix.
```

---

## Manual install

Open **Terminal.app** (Cmd+Space → type "Terminal" → Enter), paste the entire block below, press Return:

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
[ -d "$SRC" ] || git clone --recurse-submodules https://github.com/skcadri/vibeflow.git "$SRC"
cd "$SRC"

# 5. Download whisper large-v3-turbo (~1.6 GB, one-time)
bash scripts/download-model.sh

# 6. Build, bundle, code-sign
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

---

## After rebuilding — reset TCC permissions

macOS revokes Accessibility and Microphone permissions when the code signature hash changes, which happens on every rebuild. After reinstalling, run:

```bash
osascript -e 'tell application "VibeFlow" to quit' 2>/dev/null
pkill -x VibeFlow 2>/dev/null
tccutil reset Accessibility com.sohaib.vibeflow
tccutil reset Microphone   com.sohaib.vibeflow
tccutil reset ListenEvent  com.sohaib.vibeflow
tccutil reset PostEvent    com.sohaib.vibeflow
```

Then relaunch and grant permissions again. Without this, you'll see silence from the mic or "Type at Cursor failed" tray messages with no obvious cause.

---

## Troubleshooting

- **App won't launch / "VibeFlow is damaged" alert**
  ```bash
  xattr -dr com.apple.quarantine /Applications/VibeFlow.app
  ```
- **Mic delivers silence after a rebuild** — TCC permission revoked. Run the reset above and re-grant.
- **"Type at Cursor failed" tray message** — Accessibility permission revoked. Re-enable VibeFlow in System Settings → Privacy & Security → Accessibility.
- **See full diagnostic logs** — run from terminal:
  ```bash
  /Applications/VibeFlow.app/Contents/MacOS/VibeFlow 2>&1 | tee /tmp/vf.log
  ```
- **Whisper.cpp submodule out of date** — `git submodule update --init --recursive` from `~/vibeflow-src`.

More detail in [`AGENTS.md`](AGENTS.md) (architecture, historical bug diagnoses) and [`CLAUDE.md`](CLAUDE.md) (agent workflow guide).
