"""Main application controller with state machine."""

import logging
import sys
import threading
from enum import Enum, auto
from typing import Optional

from PyQt6.QtWidgets import QApplication

from .config import config
from .audio.capture import AudioCapture
# Import all transcriber models
from .transcription.medasr_model import transcriber as medasr_transcriber
from .transcription.whisper_variants import (
    whisper_tiny, whisper_base, whisper_small, whisper_medium,
    whisper_large_v3, whisper_large_v3_turbo, distil_whisper_large
)
from .input.hotkeys import HotkeyListener
from .input.typer import type_text
from .ui.bubble import FloatingBubble
from .ui.tray import SystemTray

logger = logging.getLogger(__name__)


class AppState(Enum):
    """Application states."""
    IDLE = auto()
    RECORDING = auto()
    PROCESSING = auto()


class MedASRApp:
    """Main application controller."""

    def __init__(self):
        self.state = AppState.IDLE
        self._lock = threading.Lock()

        # Initialize Qt application
        self.qt_app = QApplication(sys.argv)

        # Model management - all available models
        self.models = {
            'whisper_tiny': whisper_tiny,
            'whisper_base': whisper_base,
            'whisper_small': whisper_small,
            'whisper_medium': whisper_medium,
            'whisper_large': whisper_large_v3,
            'whisper_turbo': whisper_large_v3_turbo,
            'distil_whisper': distil_whisper_large,
            'medasr': medasr_transcriber
        }
        self.current_model = "whisper_base"  # Default model
        self.transcriber = self.models[self.current_model]

        # Components
        self.audio = AudioCapture(
            sample_rate=config.get('audio.sample_rate', 16000),
            channels=config.get('audio.channels', 1),
            chunk_size=config.get('audio.chunk_size', 1024),
            on_audio=self._on_audio_chunk
        )

        self.hotkeys = HotkeyListener(
            on_activate=self._on_hotkey_activate,
            on_deactivate=self._on_hotkey_deactivate,
            on_cancel=self._on_hotkey_cancel
        )

        self.bubble: Optional[FloatingBubble] = None
        self.tray: Optional[SystemTray] = None

        # Initialize transcriber in background
        self._init_transcriber_async()

    def _init_transcriber_async(self):
        """Initialize current transcriber in background thread."""
        def init():
            try:
                if self.current_model == "medasr":
                    logger.info("Loading MedASR model...")
                    model_name = 'google/medasr'
                    device = config.get('transcription.device', 'cuda')
                    self.transcriber.model_name = model_name
                    self.transcriber.device = device
                    self.transcriber.initialize()
                    logger.info("MedASR ready! You can now use Ctrl+Win to dictate.")
                else:
                    # All Whisper variants (tiny, base, small, medium, large, turbo, distil)
                    logger.info(f"Loading {self.current_model} model (first run downloads model)...")
                    device = config.get('transcription.device', 'cuda')
                    self.transcriber.device = device
                    self.transcriber.initialize()
                    logger.info(f"{self.current_model} ready! You can now use Ctrl+Win to dictate.")
            except Exception as e:
                logger.error(f"Failed to initialize transcriber: {e}")
                logger.error("Please check that PyTorch is installed with CUDA support.")

        thread = threading.Thread(target=init, daemon=True)
        thread.start()

    def switch_model(self, model_name: str):
        """
        Switch between transcription models.

        Args:
            model_name: Model key (e.g., 'whisper_base', 'medasr', 'whisper_turbo')
        """
        with self._lock:
            if self.state != AppState.IDLE:
                logger.warning("Cannot switch models while recording/processing")
                return

            if model_name == self.current_model:
                logger.info(f"Already using {model_name} model")
                return

            if model_name not in self.models:
                logger.error(f"Unknown model: {model_name}")
                return

            logger.info(f"Switching to {model_name} model...")
            self.current_model = model_name
            self.transcriber = self.models[model_name]

            # Initialize new model if not already initialized
            if not self.transcriber.is_initialized():
                self._init_transcriber_async()
            else:
                logger.info(f"{model_name} model ready!")

    def _on_audio_chunk(self, chunk):
        """Handle audio chunk from capture."""
        if self.state == AppState.RECORDING and self.bubble:
            self.bubble.update_audio(chunk)

    def _on_hotkey_activate(self):
        """Handle Ctrl+Win press."""
        with self._lock:
            if self.state != AppState.IDLE:
                return

            # Check if model is ready
            if not self.transcriber.is_initialized():
                logger.warning("Model still loading, please wait...")
                return

            logger.info("Starting recording...")
            self.state = AppState.RECORDING

            # Start recording
            self.audio.start_recording()

            # Show bubble
            if self.bubble:
                self.bubble.set_state("recording")

    def _on_hotkey_deactivate(self):
        """Handle Ctrl+Win release."""
        with self._lock:
            if self.state != AppState.RECORDING:
                return

            logger.info("Stopping recording...")
            self.state = AppState.PROCESSING

            # Show processing state
            if self.bubble:
                self.bubble.set_state("processing")

            # Stop recording and transcribe in background
            audio_data = self.audio.stop_recording()
            threading.Thread(
                target=self._transcribe_and_type,
                args=(audio_data,),
                daemon=True
            ).start()

    def _on_hotkey_cancel(self):
        """Handle Escape press."""
        with self._lock:
            if self.state != AppState.RECORDING:
                return

            logger.info("Cancelling recording...")

            # Stop recording
            self.audio.stop_recording()

            # Hide bubble
            if self.bubble:
                self.bubble.set_state("idle")

            self.state = AppState.IDLE

    def _transcribe_and_type(self, audio_data):
        """Transcribe audio and type result."""
        try:
            duration = len(audio_data) / config.get('audio.sample_rate', 16000)
            logger.info(f"Transcribing {duration:.1f}s of audio...")

            if duration < 0.3:
                logger.info("Audio too short, skipping")
                return

            # Check if transcriber is initialized
            if not self.transcriber.is_initialized():
                logger.error("Model not ready yet - please wait for initialization to complete")
                return

            # Transcribe
            text = self.transcriber.transcribe(
                audio_data,
                sample_rate=config.get('audio.sample_rate', 16000)
            )

            if text:
                # Type the text
                logger.info(f"Typing: {text}")
                type_text(text + " ")
            else:
                logger.warning("Empty transcription result")

        except Exception as e:
            logger.error(f"Transcription error: {e}", exc_info=True)

        finally:
            with self._lock:
                self.state = AppState.IDLE
                if self.bubble:
                    self.bubble.set_state("idle")

    def run(self):
        """Run the application."""
        logger.info("Starting MedASR...")
        logger.info("Press Ctrl+Win to start/stop dictation")
        logger.info("Press Escape while recording to cancel")
        logger.info("Right-click system tray icon to switch models")

        # Start audio stream
        try:
            self.audio.start()
        except Exception as e:
            logger.error(f"Failed to start audio: {e}")
            return 1

        # Start hotkey listener
        self.hotkeys.start()

        # Create bubble (hidden initially)
        self.bubble = FloatingBubble()

        # Create system tray icon
        self.tray = SystemTray(self)
        self.tray.run()

        # Run Qt event loop
        return self.qt_app.exec()

    def cleanup(self):
        """Cleanup resources."""
        logger.info("Cleaning up...")
        self.hotkeys.stop()
        self.audio.stop()
        if self.tray:
            self.tray.stop()
