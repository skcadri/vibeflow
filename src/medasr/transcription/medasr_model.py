"""MedASR model for speech-to-text transcription."""

import logging
import os
import numpy as np
import torch
from transformers import AutoModelForCTC, AutoProcessor

logger = logging.getLogger(__name__)


class MedASRTranscriber:
    """Google MedASR transcriber for medical speech recognition."""

    def __init__(self, model_name: str = "google/medasr", device: str = "cuda"):
        """
        Initialize MedASR transcriber.

        Args:
            model_name: Hugging Face model name
            device: 'cuda' or 'cpu'
        """
        self.model_name = model_name
        self.device = device
        self.processor = None
        self.model = None
        self._initialized = False

    def _load_hf_token(self):
        """Load Hugging Face token from .env file."""
        env_path = os.path.join(os.path.dirname(__file__), '..', '..', '..', '.env')
        if os.path.exists(env_path):
            with open(env_path, 'r') as f:
                for line in f:
                    if line.startswith('HF_TOKEN='):
                        token = line.split('=', 1)[1].strip()
                        os.environ['HF_TOKEN'] = token
                        logger.info("Loaded HF token from .env")
                        return

        # Check if token already in environment
        if 'HF_TOKEN' in os.environ:
            logger.info("Using HF token from environment")
        else:
            logger.warning("No HF_TOKEN found. Authentication may fail for gated models.")

    def initialize(self):
        """Load the model and processor. Call once at startup."""
        if self._initialized:
            return

        logger.info(f"Loading MedASR model: {self.model_name}")
        logger.info(f"Device: {self.device}")

        # Load HF token from environment
        self._load_hf_token()

        try:
            # Load processor and model
            self.processor = AutoProcessor.from_pretrained(self.model_name)
            self.model = AutoModelForCTC.from_pretrained(self.model_name)

            # Move to device
            self.model.to(self.device)
            self.model.eval()

            self._initialized = True
            logger.info("MedASR model loaded successfully")

            # Log model info
            if self.device == "cuda" and torch.cuda.is_available():
                memory_allocated = torch.cuda.memory_allocated() / 1024**3
                logger.info(f"GPU memory allocated: {memory_allocated:.2f} GB")

        except Exception as e:
            logger.error(f"Failed to load MedASR model: {e}")
            raise

    def transcribe(self, audio: np.ndarray, sample_rate: int = 16000) -> str:
        """
        Transcribe audio to text.

        Args:
            audio: Float32 numpy array, mono, normalized
            sample_rate: Must be 16000 for MedASR

        Returns:
            Transcribed text

        Raises:
            RuntimeError: If model not initialized
            ValueError: If sample rate is not 16000
        """
        if not self._initialized:
            raise RuntimeError("Model not initialized. Call initialize() first.")

        if sample_rate != 16000:
            raise ValueError(f"MedASR requires 16kHz audio, got {sample_rate}Hz")

        if len(audio) == 0:
            logger.warning("Empty audio array, returning empty string")
            return ""

        try:
            # Prepare inputs
            inputs = self.processor(
                audio,
                sampling_rate=sample_rate,
                return_tensors="pt",
                padding=True
            )

            # Move to device
            inputs = {k: v.to(self.device) for k, v in inputs.items()}

            # Inference
            with torch.no_grad():
                outputs = self.model.generate(**inputs)

            # Decode
            text = self.processor.batch_decode(outputs)[0]

            # Clean up special tokens
            text = text.replace("</s>", "").replace("<s>", "").strip()

            logger.info(f"Transcribed {len(audio)/sample_rate:.1f}s: {text[:50]}...")
            return text

        except Exception as e:
            logger.error(f"Transcription failed: {e}")
            return ""

    def is_initialized(self) -> bool:
        """Check if model is initialized."""
        return self._initialized


# Global transcriber instance
transcriber = MedASRTranscriber()
