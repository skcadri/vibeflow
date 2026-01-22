"""Waveform visualization widget."""

import logging
import numpy as np
from PyQt6.QtWidgets import QWidget
from PyQt6.QtCore import Qt, QRect
from PyQt6.QtGui import QPainter, QColor, QPen

logger = logging.getLogger(__name__)


class WaveformWidget(QWidget):
    """Real-time audio waveform visualization."""

    def __init__(self, bar_count: int = 20, parent=None):
        """
        Initialize waveform widget.

        Args:
            bar_count: Number of bars to display
            parent: Parent widget
        """
        super().__init__(parent)
        self.bar_count = bar_count
        self.amplitudes = [0.0] * bar_count
        self.setMinimumSize(240, 50)

    def update_amplitudes(self, audio_chunk: np.ndarray):
        """
        Update visualization with new audio data.

        Args:
            audio_chunk: Audio data array
        """
        if len(audio_chunk) == 0:
            return

        # Calculate RMS amplitude for each bar
        chunk_size = max(1, len(audio_chunk) // self.bar_count)

        new_amplitudes = []
        for i in range(self.bar_count):
            start = i * chunk_size
            end = min(start + chunk_size, len(audio_chunk))
            if start >= len(audio_chunk):
                new_amplitudes.append(0.0)
                continue

            # Calculate RMS
            segment = audio_chunk[start:end]
            rms = np.sqrt(np.mean(segment ** 2))

            # Scale to 0-1 range (adjust multiplier based on mic sensitivity)
            normalized = min(1.0, rms * 8)
            new_amplitudes.append(normalized)

        self.amplitudes = new_amplitudes
        self.update()  # Trigger repaint

    def paintEvent(self, event):
        """Draw the waveform bars."""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        width = self.width()
        height = self.height()

        # Calculate bar dimensions
        bar_width = width / (self.bar_count * 2)  # Space between bars
        gap = bar_width

        for i, amp in enumerate(self.amplitudes):
            x = i * (bar_width + gap)
            bar_height = max(4, amp * height * 0.8)
            y = (height - bar_height) / 2

            # White bars
            painter.setBrush(QColor(255, 255, 255, 230))
            painter.setPen(Qt.PenStyle.NoPen)

            # Draw rounded rectangle
            painter.drawRoundedRect(
                int(x), int(y), int(bar_width), int(bar_height),
                2, 2
            )
