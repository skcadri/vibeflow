# Model Switching Guide

## ✅ System Tray Icon

Look for the **microphone icon in your system tray** (bottom-right of taskbar, near the clock).

## 📋 Available Models

Right-click the system tray icon to see **8 different transcription models**:

### Fast & Balanced Models (Recommended)
1. **✓ Whisper Base** (Default, Recommended)
   - Best balance of speed and accuracy
   - ~150MB download on first use
   - Perfect for everyday dictation

2. **Whisper Medium** (Better Accuracy)
   - Higher accuracy than Base
   - ~1.5GB download
   - Good for professional documents

3. **Whisper Small** (Faster)
   - Faster than Base
   - ~500MB download
   - Good for quick notes

4. **Whisper Tiny** (Fastest)
   - Fastest transcription
   - ~75MB download
   - Good for short dictations

### Premium Models (Best Quality)
5. **Large-v3-Turbo** (Best + Fast)
   - OpenAI's latest fast model
   - ~1.6GB download
   - Best combination of speed and accuracy

6. **Distil-Whisper** (Speed Champion)
   - 6x faster than Large-v3
   - ~1.5GB download
   - 99% accuracy of Large-v3

7. **Large-v3** (Best Accuracy)
   - Highest accuracy
   - ~3GB download
   - Slower but most accurate

### Specialized Models
8. **MedASR** (Medical Only)
   - Optimized for medical terminology
   - ~400MB download
   - Best for clinical notes, radiology reports
   - Requires HuggingFace token (already set up)

## 🔄 How Model Switching Works

### First Time Use
- Each model downloads once when you first select it
- Download location: `C:\code\medasr\src\models\`
- Downloads happen in background while app runs

### Memory Management
- **Only ONE model loads in memory at a time**
- Switching unloads the current model and loads the new one
- Other models stay on disk until selected

### Switch Speed
- **Instant**: If model already used in this session
- **5-30 seconds**: First time loading (depends on model size)
- Cannot switch during recording/processing

## 📊 Model Comparison

| Model | Size | Speed | Accuracy | Best For |
|-------|------|-------|----------|----------|
| Whisper Tiny | 75MB | Fastest | Good | Quick notes |
| Whisper Small | 500MB | Fast | Better | General use |
| **Whisper Base** | **150MB** | **Balanced** | **Great** | **Everyday (Default)** |
| Whisper Medium | 1.5GB | Medium | Excellent | Professional docs |
| Whisper Large-v3 | 3GB | Slower | Best | Maximum accuracy |
| Large-v3-Turbo | 1.6GB | Fast | Best | Premium choice |
| Distil-Whisper | 1.5GB | Very Fast | Near-Best | Speed + quality |
| MedASR | 400MB | Fast | Medical++ | Clinical notes only |

## 🎯 Which Model Should You Use?

### For Most People
→ **Whisper Base** (default) - Perfect balance

### If You Want Better Accuracy
→ **Whisper Medium** or **Large-v3-Turbo**

### If You Want Maximum Speed
→ **Whisper Tiny** or **Distil-Whisper**

### For Medical Dictation
→ **MedASR** (medical terms) or **Whisper Medium** (general medical)

### For Best Overall Quality
→ **Large-v3** (if you don't mind slower speed)

## ⚠️ Important Notes

1. **Cannot switch during recording** - Finish your dictation first
2. **First download takes time** - Be patient on first use of each model
3. **GPU accelerated** - All models use your RTX 3070 Ti
4. **Models persist** - Once downloaded, they stay on your disk

## 💡 Tips

- Start with **Whisper Base** - it works great for most cases
- Try **Large-v3-Turbo** or **Distil-Whisper** for an upgrade
- Switch to **MedASR** only when dictating medical terminology
- The checkmark (✓) shows which model is currently active

## 🚀 Current Status

**Whisper Base is active by default** when you start the app.

The app logs will show:
- `"whisper_base ready!"` - Base model loaded
- `"whisper_medium ready!"` - Medium model loaded
- `"medasr ready!"` - MedASR model loaded
- etc.

---

**Try it now!** Right-click the system tray icon and see all 8 models available.
