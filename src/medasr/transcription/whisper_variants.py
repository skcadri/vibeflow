"""Different Whisper model variants for better performance."""

import logging
import numpy as np
from faster_whisper import WhisperModel

logger = logging.getLogger(__name__)


class WhisperBaseTranscriber:
    """Base class for all Whisper variants."""

    def __init__(self, model_size: str, device: str = "cuda"):
        self.model_size = model_size
        self.device = device
        self.compute_type = "float16" if device == "cuda" else "int8"
        self.model = None
        self._initialized = False

    def initialize(self):
        """Load the model."""
        if self._initialized:
            return

        logger.info(f"Loading {self.model_size} model...")
        logger.info(f"Device: {self.device}, Compute type: {self.compute_type}")

        try:
            self.model = WhisperModel(
                self.model_size,
                device=self.device,
                compute_type=self.compute_type,
                download_root="./models"
            )
            self._initialized = True
            logger.info(f"{self.model_size} model loaded successfully")

        except Exception as e:
            logger.error(f"Failed to load {self.model_size}: {e}")
            raise

    def transcribe(self, audio: np.ndarray, sample_rate: int = 16000, hotwords: str = None) -> str:
        """
        Transcribe audio to text.

        Args:
            audio: Audio data array
            sample_rate: Sample rate (must be 16000)
            hotwords: Optional hotwords string for vocabulary boost
        """
        if not self._initialized:
            raise RuntimeError("Model not initialized")

        if sample_rate != 16000:
            raise ValueError(f"Whisper requires 16kHz audio, got {sample_rate}Hz")

        if len(audio) == 0:
            return ""

        try:
            segments, info = self.model.transcribe(
                audio,
                language="en",
                vad_filter=True,
                vad_parameters=dict(min_silence_duration_ms=500),
                hotwords=hotwords
            )

            text_parts = [segment.text.strip() for segment in segments]
            text = " ".join(text_parts)

            logger.info(f"Transcribed {len(audio)/sample_rate:.1f}s: {text[:50]}...")
            return text

        except Exception as e:
            logger.error(f"Transcription failed: {e}")
            return ""

    def is_initialized(self) -> bool:
        return self._initialized


# Individual model instances
class WhisperTiny(WhisperBaseTranscriber):
    """Whisper tiny.en - Fastest, lowest accuracy."""
    def __init__(self, device: str = "cuda"):
        super().__init__("tiny.en", device)


class WhisperBase(WhisperBaseTranscriber):
    """Whisper base.en - Good balance."""
    def __init__(self, device: str = "cuda"):
        super().__init__("base.en", device)


class WhisperSmall(WhisperBaseTranscriber):
    """Whisper small.en - Better accuracy."""
    def __init__(self, device: str = "cuda"):
        super().__init__("small.en", device)


class WhisperMedium(WhisperBaseTranscriber):
    """Whisper medium.en - High accuracy."""
    def __init__(self, device: str = "cuda"):
        super().__init__("medium.en", device)


class WhisperLargeV3(WhisperBaseTranscriber):
    """Whisper large-v3 - Best accuracy, slower."""
    def __init__(self, device: str = "cuda"):
        super().__init__("large-v3", device)


class WhisperLargeV3Turbo(WhisperBaseTranscriber):
    """Whisper large-v3-turbo - Best accuracy, fast."""
    def __init__(self, device: str = "cuda"):
        super().__init__("large-v3-turbo", device)


class DistilWhisperLarge(WhisperBaseTranscriber):
    """Distil-Whisper large-v3 - Fast with high accuracy."""
    def __init__(self, device: str = "cuda"):
        super().__init__("distil-large-v3", device)


# Global instances (lazy-loaded)
whisper_tiny = WhisperTiny()
whisper_base = WhisperBase()
whisper_small = WhisperSmall()
whisper_medium = WhisperMedium()
whisper_large_v3 = WhisperLargeV3()
whisper_large_v3_turbo = WhisperLargeV3Turbo()
distil_whisper_large = DistilWhisperLarge()
