"""Text typing simulation using pynput."""

import logging
from pynput.keyboard import Controller

logger = logging.getLogger(__name__)

# Global keyboard controller
_keyboard = Controller()


def type_text(text: str):
    """
    Type text at current cursor position.

    Args:
        text: Text to type
    """
    if not text:
        return

    try:
        logger.info(f"Typing text: {text[:50]}{'...' if len(text) > 50 else ''}")
        _keyboard.type(text)
    except Exception as e:
        logger.error(f"Failed to type text: {e}")
