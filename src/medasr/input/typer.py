"""Text input simulation using clipboard paste or keyboard typing."""

import logging
import time
import pyperclip
from pynput.keyboard import Controller, Key

logger = logging.getLogger(__name__)

# Global keyboard controller
_keyboard = Controller()


def type_text(text: str, method: str = 'paste'):
    """
    Enter text at current cursor position.

    Args:
        text: Text to enter
        method: 'paste' for clipboard-based paste (default),
                'type' for character-by-character keyboard typing
    """
    if not text:
        return

    if method == 'type':
        _type_text_keyboard(text)
    else:
        _paste_text_clipboard(text)


def _paste_text_clipboard(text: str):
    """
    Paste text at current cursor position using clipboard.

    Uses clipboard + Ctrl+V instead of typing character-by-character
    to avoid issues with apps like WhatsApp where Enter sends a message.
    """
    try:
        logger.info(f"Pasting text: {text[:50]}{'...' if len(text) > 50 else ''}")

        # Save current clipboard content
        try:
            old_clipboard = pyperclip.paste()
        except Exception:
            old_clipboard = ""

        # Copy our text to clipboard
        pyperclip.copy(text)

        # Small delay to ensure clipboard is ready
        time.sleep(0.05)

        # Simulate Ctrl+V to paste
        _keyboard.press(Key.ctrl)
        _keyboard.press('v')
        _keyboard.release('v')
        _keyboard.release(Key.ctrl)

        # Small delay before restoring clipboard
        time.sleep(0.1)

        # Restore original clipboard content
        try:
            pyperclip.copy(old_clipboard)
        except Exception:
            pass  # Don't fail if we can't restore clipboard

    except Exception as e:
        logger.error(f"Failed to paste text: {e}")
        # Fallback to typing if paste fails
        try:
            _keyboard.type(text)
        except Exception as e2:
            logger.error(f"Fallback typing also failed: {e2}")


def _type_text_keyboard(text: str):
    """
    Type text character-by-character using keyboard simulation.

    Slower but doesn't touch the clipboard.
    """
    try:
        logger.info(f"Typing text: {text[:50]}{'...' if len(text) > 50 else ''}")
        _keyboard.type(text)
    except Exception as e:
        logger.error(f"Failed to type text: {e}")
