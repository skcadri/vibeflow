"""Audio capture using sounddevice."""

import logging
import queue
import threading
from typing import Callable, Optional
import numpy as np
import sounddevice as sd

logger = logging.getLogger(__name__)


class AudioCapture:
    """Real-time audio capture using sounddevice."""

    def __init__(
        self,
        sample_rate: int = 16000,
        channels: int = 1,
        chunk_size: int = 1024,
        on_audio: Optional[Callable[[np.ndarray], None]] = None
    ):
        """
        Initialize audio capture.

        Args:
            sample_rate: Sample rate in Hz (16000 for MedASR)
            channels: Number of channels (1 = mono)
            chunk_size: Frames per buffer
            on_audio: Optional callback for each audio chunk
        """
        self.sample_rate = sample_rate
        self.channels = channels
        self.chunk_size = chunk_size
        self.on_audio = on_audio

        self.audio_queue: queue.Queue[np.ndarray] = queue.Queue()
        self.stream: Optional[sd.InputStream] = None
        self.is_recording = False
        self._lock = threading.Lock()
        self._buffer = []

    def _audio_callback(self, indata: np.ndarray, frames: int, time_info, status: sd.CallbackFlags):
        """Callback for audio stream."""
        if status:
            logger.warning(f"Audio status: {status}")

        if self.is_recording:
            # Convert to float32 mono
            audio_chunk = indata[:, 0].copy() if indata.ndim > 1 else indata.copy()
            audio_chunk = audio_chunk.astype(np.float32)

            # Store in buffer
            self._buffer.append(audio_chunk)

            # Queue for processing
            self.audio_queue.put(audio_chunk)

            # Call callback if provided
            if self.on_audio:
                self.on_audio(audio_chunk)

    def start(self):
        """Start the audio stream."""
        with self._lock:
            if self.stream is not None:
                return

            logger.info("Starting audio capture...")
            logger.info(f"Sample rate: {self.sample_rate}Hz, Channels: {self.channels}")

            try:
                self.stream = sd.InputStream(
                    samplerate=self.sample_rate,
                    channels=self.channels,
                    dtype=np.float32,
                    blocksize=self.chunk_size,
                    callback=self._audio_callback
                )
                self.stream.start()
                logger.info("Audio stream started")
            except Exception as e:
                logger.error(f"Failed to start audio stream: {e}")
                self.stream = None
                raise

    def stop(self):
        """Stop the audio stream."""
        with self._lock:
            self.is_recording = False
            if self.stream is not None:
                self.stream.stop()
                self.stream.close()
                self.stream = None
                logger.info("Audio stream stopped")

    def start_recording(self):
        """Start recording audio."""
        self.is_recording = True
        self._buffer.clear()
        self.clear_queue()
        logger.debug("Recording started")

    def stop_recording(self) -> np.ndarray:
        """
        Stop recording and return captured audio.

        Returns:
            Numpy array of recorded audio (float32, mono)
        """
        self.is_recording = False
        logger.debug("Recording stopped")

        # Concatenate all chunks
        if len(self._buffer) == 0:
            return np.array([], dtype=np.float32)

        audio = np.concatenate(self._buffer)
        self._buffer.clear()
        return audio

    def clear_queue(self):
        """Clear any pending audio in queue."""
        while not self.audio_queue.empty():
            try:
                self.audio_queue.get_nowait()
            except queue.Empty:
                break

    @staticmethod
    def list_devices() -> list[dict]:
        """List available audio input devices."""
        devices = []
        for i, device in enumerate(sd.query_devices()):
            if device['max_input_channels'] > 0:
                devices.append({
                    'index': i,
                    'name': device['name'],
                    'channels': device['max_input_channels'],
                    'sample_rate': device['default_samplerate']
                })
        return devices
