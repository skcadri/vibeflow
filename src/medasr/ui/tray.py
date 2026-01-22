"""System tray icon with model switching."""

import logging
from PIL import Image, ImageDraw
import pystray
from pystray import MenuItem as item
import threading

logger = logging.getLogger(__name__)


def create_icon():
    """Create a simple microphone icon."""
    width = 64
    height = 64
    image = Image.new('RGB', (width, height), 'white')
    dc = ImageDraw.Draw(image)

    # Draw microphone shape
    # Mic body
    dc.ellipse([20, 15, 44, 40], fill='black')
    # Mic stand
    dc.rectangle([30, 40, 34, 55], fill='black')
    # Mic base
    dc.rectangle([20, 55, 44, 58], fill='black')

    return image


class SystemTray:
    """System tray icon with model selection menu."""

    def __init__(self, app):
        """
        Initialize system tray.

        Args:
            app: Reference to main app for callbacks
        """
        self.app = app
        self.icon = None
        self.current_model = "whisper_base"  # Default
        self.on_settings_open = None  # Callback for double-click
        self._thread = None  # Thread reference for cleanup

    def _create_model_switcher(self, model_key: str, display_name: str):
        """Create a model switch callback."""
        def switch():
            logger.info(f"Switching to {display_name}...")
            self.app.switch_model(model_key)
            self.current_model = model_key
            self._update_menu()
        return switch

    def _on_settings(self):
        """Open settings window."""
        logger.info("Settings requested from tray")
        if self.on_settings_open:
            self.on_settings_open()

    def _on_quit(self):
        """Quit the application."""
        logger.info("Quit requested from tray")
        # Cleanup first (which will call our stop() method)
        self.app.cleanup()
        # Then quit Qt
        self.app.qt_app.quit()

    def _update_menu(self):
        """Update menu to show current model."""
        if self.icon:
            self.icon.menu = self._create_menu()

    def _create_menu(self):
        """Create the system tray menu."""
        return pystray.Menu(
            # Settings at the top
            item('Settings...', self._on_settings),
            pystray.Menu.SEPARATOR,
            # Whisper Models - Fast & Balanced
            item(
                'Whisper Base (Recommended)',
                self._create_model_switcher('whisper_base', 'Whisper Base'),
                checked=lambda item: self.current_model == 'whisper_base'
            ),
            item(
                'Whisper Medium (Better Accuracy)',
                self._create_model_switcher('whisper_medium', 'Whisper Medium'),
                checked=lambda item: self.current_model == 'whisper_medium'
            ),
            item(
                'Whisper Small (Faster)',
                self._create_model_switcher('whisper_small', 'Whisper Small'),
                checked=lambda item: self.current_model == 'whisper_small'
            ),
            item(
                'Whisper Tiny (Fastest)',
                self._create_model_switcher('whisper_tiny', 'Whisper Tiny'),
                checked=lambda item: self.current_model == 'whisper_tiny'
            ),
            pystray.Menu.SEPARATOR,
            # Premium Models - Best Quality
            item(
                'Large-v3-Turbo (Best + Fast)',
                self._create_model_switcher('whisper_turbo', 'Whisper Turbo'),
                checked=lambda item: self.current_model == 'whisper_turbo'
            ),
            item(
                'Distil-Whisper (Speed Champion)',
                self._create_model_switcher('distil_whisper', 'Distil Whisper'),
                checked=lambda item: self.current_model == 'distil_whisper'
            ),
            item(
                'Large-v3 (Best Accuracy)',
                self._create_model_switcher('whisper_large', 'Whisper Large'),
                checked=lambda item: self.current_model == 'whisper_large'
            ),
            pystray.Menu.SEPARATOR,
            # Medical Model
            item(
                'MedASR (Medical Only)',
                self._create_model_switcher('medasr', 'MedASR'),
                checked=lambda item: self.current_model == 'medasr'
            ),
            pystray.Menu.SEPARATOR,
            item('Quit', self._on_quit)
        )

    def _on_double_click(self):
        """Handle double-click on tray icon."""
        if self.on_settings_open:
            self.on_settings_open()

    def run(self):
        """Start the system tray icon."""
        icon_image = create_icon()
        self.icon = pystray.Icon(
            "MedASR",
            icon_image,
            "MedASR - Voice Dictation",
            menu=self._create_menu()
        )

        # Set double-click handler (on_activate in pystray)
        # Note: on Windows, this fires on left-click, not double-click
        self.icon.on_activate = self._on_double_click

        # Run in separate thread
        self._thread = threading.Thread(target=self.icon.run, daemon=True)
        self._thread.start()
        logger.info("System tray icon started")

    def stop(self):
        """Stop the system tray icon."""
        if self.icon and self.icon.visible:
            logger.info("Stopping system tray icon...")
            try:
                # Ensure icon is marked as not visible
                self.icon.visible = False
                # Stop the icon loop
                self.icon.stop()
                logger.info("System tray icon stopped")
            except Exception as e:
                logger.error(f"Error stopping tray icon: {e}")
        else:
            logger.debug("Tray icon already stopped or not visible")
