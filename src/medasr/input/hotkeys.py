"""Global hotkey listener using pynput."""

import logging
from typing import Callable, Optional
from pynput import keyboard

logger = logging.getLogger(__name__)


class HotkeyListener:
    """Global hotkey listener for Ctrl+Win activation."""

    def __init__(
        self,
        on_activate: Optional[Callable[[], None]] = None,
        on_deactivate: Optional[Callable[[], None]] = None,
        on_cancel: Optional[Callable[[], None]] = None
    ):
        """
        Initialize hotkey listener.

        Args:
            on_activate: Callback when Ctrl+Win pressed
            on_deactivate: Callback when Ctrl+Win released
            on_cancel: Callback when Escape pressed
        """
        self.on_activate = on_activate
        self.on_deactivate = on_deactivate
        self.on_cancel = on_cancel

        self.ctrl_pressed = False
        self.win_pressed = False
        self._active = False
        self._listener: Optional[keyboard.Listener] = None

    def _on_press(self, key):
        """Handle key press events."""
        try:
            # Track Ctrl keys
            if key in (keyboard.Key.ctrl_l, keyboard.Key.ctrl_r, keyboard.Key.ctrl):
                self.ctrl_pressed = True

            # Track Win key (cmd on pynput)
            elif key == keyboard.Key.cmd or key == keyboard.Key.cmd_l or key == keyboard.Key.cmd_r:
                self.win_pressed = True

            # Track Escape key
            elif key == keyboard.Key.esc:
                if self._active and self.on_cancel:
                    logger.debug("Escape pressed - cancelling")
                    self.on_cancel()

            # Check if Ctrl+Win combo is pressed
            if self.ctrl_pressed and self.win_pressed and not self._active:
                self._active = True
                logger.debug("Ctrl+Win activated")
                if self.on_activate:
                    self.on_activate()

        except AttributeError:
            # Handle key.char if key doesn't have the expected attributes
            pass

    def _on_release(self, key):
        """Handle key release events."""
        try:
            # Track Ctrl release
            if key in (keyboard.Key.ctrl_l, keyboard.Key.ctrl_r, keyboard.Key.ctrl):
                self.ctrl_pressed = False

            # Track Win release
            elif key == keyboard.Key.cmd or key == keyboard.Key.cmd_l or key == keyboard.Key.cmd_r:
                self.win_pressed = False

            # Check if Ctrl+Win combo is released
            if self._active and not (self.ctrl_pressed and self.win_pressed):
                self._active = False
                logger.debug("Ctrl+Win deactivated")
                if self.on_deactivate:
                    self.on_deactivate()

        except AttributeError:
            pass

    def start(self):
        """Start listening for hotkeys."""
        if self._listener is not None:
            return

        logger.info("Starting hotkey listener (Ctrl+Win)")
        self._listener = keyboard.Listener(
            on_press=self._on_press,
            on_release=self._on_release
        )
        self._listener.start()

    def stop(self):
        """Stop listening for hotkeys."""
        if self._listener is not None:
            self._listener.stop()
            self._listener = None
            logger.info("Hotkey listener stopped")
