"""Parakeet ONNX model for speech-to-text transcription."""

import logging
from typing import Any, List

import numpy as np
from onnx_asr import load_model

try:
    import onnxruntime as ort
except Exception:  # pragma: no cover
    ort = None

logger = logging.getLogger(__name__)


class ParakeetOnnxTranscriber:
    """NVIDIA Parakeet-TDT transcriber via ONNX runtime."""

    def __init__(self, model_name: str = "nemo-parakeet-tdt-0.6b-v2", device: str = "cuda"):
        self.model_name = model_name
        self.device = device
        self.model: Any = None
        self._initialized = False

    def _available_providers(self) -> List[str]:
        if ort is None:
            return ["CPUExecutionProvider"]
        try:
            return list(ort.get_available_providers())
        except Exception:
            return ["CPUExecutionProvider"]

    def _providers(self) -> List[str]:
        available = set(self._available_providers())
        if self.device == "cuda":
            preferred = ["CUDAExecutionProvider", "CPUExecutionProvider"]
            providers = [p for p in preferred if p in available]
            if "CUDAExecutionProvider" not in providers:
                logger.warning("CUDA requested but CUDAExecutionProvider not available; using CPU")
            return providers or ["CPUExecutionProvider"]
        return ["CPUExecutionProvider"]

    def initialize(self):
        """Load ONNX model. Call once at startup."""
        if self._initialized:
            return

        logger.info(f"Loading Parakeet model: {self.model_name}")
        logger.info(f"Device: {self.device}")

        try:
            self.model = load_model(self.model_name, providers=self._providers())
            self._initialized = True
            logger.info("Parakeet model loaded successfully")
        except Exception as e:
            logger.error(f"Failed to load Parakeet model: {e}")
            raise

    def transcribe(self, audio: np.ndarray, sample_rate: int = 16000, hotwords: str = None) -> str:
        """Transcribe audio to text."""
        del hotwords  # Not supported by Parakeet ONNX path

        if not self._initialized:
            raise RuntimeError("Model not initialized. Call initialize() first.")

        if sample_rate != 16000:
            raise ValueError(f"Parakeet requires 16kHz audio, got {sample_rate}Hz")

        if len(audio) == 0:
            logger.warning("Empty audio array, returning empty string")
            return ""

        if audio.dtype != np.float32:
            audio = audio.astype(np.float32)

        try:
            output = self.model.recognize(audio, sample_rate=sample_rate)

            if isinstance(output, str):
                text = output.strip()
            elif hasattr(output, "text"):
                text = str(output.text).strip()
            else:
                text = str(output).strip()

            logger.info(f"Transcribed {len(audio)/sample_rate:.1f}s: {text[:50]}...")
            return text
        except Exception as e:
            logger.error(f"Transcription failed: {e}")
            return ""

    def is_initialized(self) -> bool:
        """Check if model is initialized."""
        return self._initialized

    def unload(self):
        """Unload model from memory."""
        if self.model is not None:
            logger.info("Unloading Parakeet model from memory...")
            del self.model
            self.model = None
            self._initialized = False
            logger.info("Parakeet model unloaded")


# Global transcriber instance
transcriber = ParakeetOnnxTranscriber()
