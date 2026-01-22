"""Faster-whisper model for speech-to-text transcription."""

import logging
import numpy as np
from faster_whisper import WhisperModel

logger = logging.getLogger(__name__)


class FasterWhisperTranscriber:
    """Faster-whisper transcriber for general speech recognition."""

    def __init__(self, model_size: str = "base.en", device: str = "cuda"):
        """
        Initialize faster-whisper transcriber.

        Args:
            model_size: Model size (tiny.en, base.en, small.en, medium.en, large-v3)
            device: 'cuda' or 'cpu'
        """
        self.model_size = model_size
        self.device = device
        self.compute_type = "float16" if device == "cuda" else "int8"
        self.model = None
        self._initialized = False

    def initialize(self):
        """Load the model. Call once at startup."""
        if self._initialized:
            return

        logger.info(f"Loading faster-whisper model: {self.model_size}")
        logger.info(f"Device: {self.device}, Compute type: {self.compute_type}")

        try:
            self.model = WhisperModel(
                self.model_size,
                device=self.device,
                compute_type=self.compute_type,
                download_root="./models"
            )

            self._initialized = True
            logger.info("Faster-whisper model loaded successfully")

        except Exception as e:
            logger.error(f"Failed to load faster-whisper model: {e}")
            raise

    def transcribe(self, audio: np.ndarray, sample_rate: int = 16000) -> str:
        """
        Transcribe audio to text.

        Args:
            audio: Float32 numpy array, mono, normalized
            sample_rate: Must be 16000 for Whisper

        Returns:
            Transcribed text
        """
        if not self._initialized:
            raise RuntimeError("Model not initialized. Call initialize() first.")

        if sample_rate != 16000:
            raise ValueError(f"Whisper requires 16kHz audio, got {sample_rate}Hz")

        if len(audio) == 0:
            logger.warning("Empty audio array, returning empty string")
            return ""

        try:
            # Transcribe with VAD
            segments, info = self.model.transcribe(
                audio,
                language="en",
                vad_filter=True,
                vad_parameters=dict(
                    min_silence_duration_ms=500
                )
            )

            # Collect all segments
            text_parts = []
            for segment in segments:
                text_parts.append(segment.text.strip())

            text = " ".join(text_parts)

            logger.info(f"Transcribed {len(audio)/sample_rate:.1f}s: {text[:50]}...")
            return text

        except Exception as e:
            logger.error(f"Transcription failed: {e}")
            return ""

    def is_initialized(self) -> bool:
        """Check if model is initialized."""
        return self._initialized


# Global transcriber instance
whisper_transcriber = FasterWhisperTranscriber()
