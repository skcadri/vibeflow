# CLAUDE.md — VibeFlow Project Guide for Claude

This file is the first thing a Claude agent should read when picking up VibeFlow. It points to the deeper architecture doc (`AGENTS.md`) and captures workflow knowledge that isn't obvious from the code.

## What is this project

VibeFlow is a native macOS voice dictation app (whisper.cpp + Qt6 + Metal GPU on Apple Silicon). Hold Cmd+Ctrl, speak, release → text appears at the cursor. See `AGENTS.md` for the full architecture, state machine, and file-by-file reference.

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

Always quit any running VibeFlow process first (`pkill -x VibeFlow`). Then refresh the home dir model symlink if it's a symlink to the bundle:

```bash
ln -sf /Applications/VibeFlow.app/Contents/Resources/ggml-large-v3-turbo.bin \
       ~/vibeflow/models/ggml-large-v3-turbo.bin
```

## Model file layout

- **Canonical bytes** live at `/Applications/VibeFlow.app/Contents/Resources/ggml-large-v3-turbo.bin` (1.5 GB)
- `~/vibeflow/models/ggml-large-v3-turbo.bin` is a symlink → bundle (so the dev search path in `App.cpp` resolves)
- Project `models/` directory is gitignored; populated by `scripts/download-model.sh` or by APFS clone (`cp -c`) from the bundle when needed for a rebuild

If the bundle ever gets nuked, the home dir symlink dangles and the next build has no source for `MODEL_SRC`. Re-download via `bash scripts/download-model.sh` to recover.

## Code signing

The build script auto-selects the `"VibeFlow Dev"` signing identity if present, otherwise falls back to ad-hoc (`-`). Stable identity matters because TCC tracks permission per-signature; the README/AGENTS.md document the ad-hoc footgun. For your own builds, leave the default — the identity is on the user's keychain.

## Where else to look

- **`AGENTS.md`** — full architecture, file-by-file reference, the audio-capture-returns-silence bug history, manual build instructions, diagnostic logging tips
- **`scripts/build.sh`** — the canonical build pipeline (cmake → macdeployqt → rpath fix → codesign → model copy)
- **`scripts/download-model.sh`** — pulls `ggml-large-v3-turbo.bin` from Hugging Face

## About the user

Sohaib (skcadri) is a neurologist, not a formally-trained developer. Explain technical decisions accessibly. Prefer concrete commands and visual artifacts (HTML) over jargon-heavy prose. Default to `main` for committed work; he uses Conductor for parallel branches.
