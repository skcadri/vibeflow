"""System tray icon with model switching."""

import logging
from PIL import Image, ImageDraw
import pystray
from pystray import MenuItem as item
import threading

from ..config import config

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
        self.current_model = getattr(app, 'current_model', 'parakeet_tdt')
        self.current_device = config.get('transcription.device', 'cpu')  # Load from config
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

    def _toggle_device(self):
        """Toggle between CPU and CUDA."""
        new_device = "cuda" if self.current_device == "cpu" else "cpu"
        logger.info(f"Toggling device to {new_device}...")
        self.app.switch_device(new_device)
        self.current_device = new_device
        self._update_menu()

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
        device_label = "Use GPU (CUDA)" if self.current_device == "cpu" else "Use CPU"
        return pystray.Menu(
            # Settings at the top
            item('Settings...', self._on_settings),
            pystray.Menu.SEPARATOR,
            # Device toggle
            item(device_label, self._toggle_device),
            pystray.Menu.SEPARATOR,
            item(
                'Parakeet TDT v2 (Recommended)',
                self._create_model_switcher('parakeet_tdt', 'Parakeet TDT v2'),
                checked=lambda item: self.current_model == 'parakeet_tdt'
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
