"""Floating bubble UI window."""

import logging
from PyQt6.QtWidgets import QWidget, QVBoxLayout, QApplication
from PyQt6.QtCore import Qt, QTimer, pyqtSignal, QSize
from PyQt6.QtGui import QPainter, QColor, QPainterPath, QBrush

from .waveform import WaveformWidget

logger = logging.getLogger(__name__)


class FloatingBubble(QWidget):
    """Floating UI bubble with waveform visualization."""

    # Signals for thread-safe updates
    state_changed_signal = pyqtSignal(str)
    audio_received_signal = pyqtSignal(object)

    def __init__(self):
        super().__init__()
        self._state = "idle"
        self._setup_window()
        self._setup_ui()

        # Connect signals
        self.state_changed_signal.connect(self._on_state_changed)
        self.audio_received_signal.connect(self._on_audio_received)

    def _setup_window(self):
        """Configure window flags for floating behavior."""
        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint |
            Qt.WindowType.WindowStaysOnTopHint |
            Qt.WindowType.Tool  # Don't show in taskbar
        )
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)
        self.setAttribute(Qt.WidgetAttribute.WA_ShowWithoutActivating)

        # Initial size (collapsed)
        self.setFixedSize(60, 60)

    def _setup_ui(self):
        """Create UI components."""
        self.layout = QVBoxLayout(self)
        self.layout.setContentsMargins(5, 5, 5, 5)

        # Waveform (hidden initially)
        self.waveform = WaveformWidget(bar_count=20, parent=self)
        self.waveform.hide()
        self.layout.addWidget(self.waveform)

    def paintEvent(self, event):
        """Draw rounded background."""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        # Rounded rectangle background
        path = QPainterPath()
        path.addRoundedRect(0, 0, self.width(), self.height(), 15, 15)

        # Color based on state
        if self._state == "recording":
            color = QColor(220, 53, 69, 230)  # Red
        elif self._state == "processing":
            color = QColor(255, 193, 7, 230)  # Yellow/Amber
        else:
            color = QColor(40, 40, 40, 200)  # Dark gray

        painter.fillPath(path, QBrush(color))

    def _on_state_changed(self, state: str):
        """Handle state change (thread-safe via signal)."""
        self._state = state

        if state == "recording":
            # Expand and show waveform
            self.setFixedSize(260, 70)
            self.waveform.show()
            self.position_bottom_center()
            if not self.isVisible():
                self.show()
        elif state == "processing":
            # Keep expanded, change color
            pass
        else:
            # Collapse and hide
            self.setFixedSize(60, 60)
            self.waveform.hide()
            self.hide()

        self.update()

    def _on_audio_received(self, audio_chunk):
        """Update waveform with audio (thread-safe via signal)."""
        if self._state == "recording":
            self.waveform.update_amplitudes(audio_chunk)

    def set_state(self, state: str):
        """
        Change bubble state.

        Args:
            state: "idle", "recording", or "processing"
        """
        self.state_changed_signal.emit(state)

    def update_audio(self, audio_chunk):
        """
        Update waveform visualization.

        Args:
            audio_chunk: Audio data array
        """
        self.audio_received_signal.emit(audio_chunk)

    def position_bottom_center(self):
        """Position bubble at bottom-center of screen."""
        screen = QApplication.primaryScreen()
        if screen:
            geom = screen.geometry()
            x = (geom.width() - self.width()) // 2
            y = geom.height() - self.height() - 50  # 50px from bottom
            self.move(x, y)
