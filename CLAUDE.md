# CLAUDE.md — VibeFlow Project Guide for Claude

This file is the first thing a Claude agent should read when picking up VibeFlow. It points to the deeper architecture doc (`AGENTS.md`) and captures workflow knowledge that isn't obvious from the code.

## What is this project

VibeFlow is a native macOS voice dictation app (transcribe.cpp running Qwen3-ASR + Whisper, Qt6, Metal GPU on Apple Silicon). Hold Cmd+Ctrl, speak, release → text appears at the cursor. See `AGENTS.md` for the full architecture, state machine, and file-by-file reference.

## Critical workflow gotchas

### 1. Always check `git worktree list` before any build/install workflow

This repo's real working state is often **not on `main`**. The user (skcadri) develops in **Conductor workspaces** under `~/conductor/workspaces/vibeflow/<name>/`, each on its own feature branch. Before doing any build, install, or merge, run:

```bash
git worktree list
git branch -a -v
```

If a Conductor worktree's branch is ahead of `main`, that's where the user has been working. Building from `main` instead will silently regress real features.

### 2. ALWAYS reset TCC permissions after reinstalling /Applications/VibeFlow.app

macOS Tahoe revokes Accessibility/Microphone permissions when the code signature hash changes, which it does on every rebuild — even with a stable signing identity. After every install, run:

```bash
osascript -e 'tell application "VibeFlow" to quit' 2>/dev/null
pkill -x VibeFlow 2>/dev/null
tccutil reset Accessibility com.sohaib.vibeflow
tccutil reset Microphone   com.sohaib.vibeflow
tccutil reset ListenEvent  com.sohaib.vibeflow
tccutil reset PostEvent    com.sohaib.vibeflow
```

Then tell the user to:
1. Launch the app
2. Click **Allow** on the microphone prompt
3. Open System Settings → Privacy & Security → Accessibility, find **VibeFlow**, toggle it ON

(`open "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"` deep-links there.)

If you skip the reset, the user will see "Type at Cursor failed" or hear silence from the mic, and won't know why.

### 3. Standard install command

The build script's documented install line:

```bash
rm -rf /Applications/VibeFlow.app && cp -R build/VibeFlow.app /Applications/
```

Always quit any running VibeFlow process first (`pkill -x VibeFlow`).

To capture the app's stderr log (hotkey ACTIVATED/DEACTIVATED with modifier flags, model loads, paste results), launch it with `open --stderr ~/vibeflow/logs/vibeflow.log /Applications/VibeFlow.app`. Don't run the binary from a shell for this: macOS then attributes mic/Accessibility permission to the terminal. For when recording started/stopped in the past, `/usr/bin/log show --predicate 'process == "VibeFlow" AND eventMessage CONTAINS "setPlayState"'` lists every mic start/stop (zsh shadows `log` with a builtin; use the full path).

## Model file layout

Three GGUF models. The first two are required (the app refuses to start dictation if either is missing); the third is optional:

| File | Size | Used for |
|---|---|---|
| `Qwen3-ASR-1.7B-Q8_0.gguf` | 2.2 GB | Hotkey dictation |
| `whisper-large-v3-turbo-Q8_0.gguf` | 0.9 GB | Urdu (Qwen has none) and the HTTP server (needs timestamps) |
| `nemotron-speech-streaming-en-0.6b-Q8_0.gguf` | 0.7 GB | Live text panel while speaking (preview only, English-only). Missing = no live text, nothing else changes |

- **Canonical bytes** live in `~/vibeflow/models/` (real files, not symlinks). `App.cpp` searches the app bundle's `Resources/` first, then `~/vibeflow/models/`, then `~/.vibeflow/models/`.
- Project `models/` is gitignored. Before a build, populate it with `cp -c ~/vibeflow/models/*.gguf models/` (APFS clone, no extra disk) or `bash scripts/download-model.sh`. `build.sh` clones them into the bundle.
- Never use the old whisper.cpp `ggml-*.bin` files — transcribe.cpp only reads its own GGUFs from `huggingface.co/handy-computer`.

## Live text preview

- It's a preview: the pasted text always comes from Qwen's full pass on release. Never paste or save `LivePreview` output.
- Off switch for the user: tray menu → **Show Live Text While Speaking** (persisted). Use that before touching code when the user reports a live-text problem.

## Code signing

The build script auto-selects the `"VibeFlow Dev"` signing identity if present, otherwise falls back to ad-hoc (`-`). Stable identity matters because TCC tracks permission per-signature; the README/AGENTS.md document the ad-hoc footgun. For your own builds, leave the default — the identity is on the user's keychain.

## Where else to look

- **`AGENTS.md`** — full architecture, file-by-file reference, the audio-capture-returns-silence bug history, manual build instructions, diagnostic logging tips
- **`scripts/build.sh`** — the canonical build pipeline (cmake → macdeployqt → rpath fix → codesign → model copy)
- **`scripts/download-model.sh`** — pulls both GGUF models from Hugging Face
- **`deps/transcribe.cpp/docs/`** — engine API (`include/transcribe.h`), per-model docs, prompting rules

## About the user

Sohaib (skcadri) is a neurologist, not a formally-trained developer. Explain technical decisions accessibly. Prefer concrete commands and visual artifacts (HTML) over jargon-heavy prose. Default to `main` for committed work; he uses Conductor for parallel branches.
