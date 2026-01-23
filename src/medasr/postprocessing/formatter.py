"""Local LLM text formatter using Qwen2-0.5B for post-processing."""

import gc
import logging
import threading
from typing import Optional

logger = logging.getLogger(__name__)

# Formatting prompt for the LLM
FORMATTING_PROMPT = """<|im_start|>system
You format transcribed speech for typing. Fix capitalization and punctuation. Convert lists to bullet points with "- " prefix if appropriate. Add paragraph breaks if needed. Keep the meaning exactly the same. Output ONLY the formatted text, nothing else.<|im_end|>
<|im_start|>user
{text}<|im_end|>
<|im_start|>assistant
"""


class LocalLLMFormatter:
    """Formats transcribed text using a local Qwen2-0.5B model."""

    def __init__(self):
        self.model = None
        self._initialized = False
        self._lock = threading.RLock()

    def initialize(self):
        """Load the model into VRAM."""
        with self._lock:
            if self._initialized:
                logger.info("Formatter already initialized")
                return True

            try:
                logger.info("Loading Qwen2-0.5B formatter model...")
                from llama_cpp import Llama

                self.model = Llama.from_pretrained(
                    repo_id="Qwen/Qwen2-0.5B-Instruct-GGUF",
                    filename="qwen2-0_5b-instruct-q4_k_m.gguf",
                    n_ctx=512,
                    n_gpu_layers=-1,  # All layers on GPU
                    verbose=False
                )
                self._initialized = True
                logger.info("Formatter model loaded successfully")
                return True

            except Exception as e:
                logger.error(f"Failed to load formatter model: {e}")
                self._initialized = False
                return False

    def unload(self):
        """Unload model from VRAM."""
        with self._lock:
            if not self._initialized:
                return

            logger.info("Unloading formatter model...")
            if self.model:
                del self.model
                self.model = None

            self._initialized = False

            # Force garbage collection and clear GPU memory
            gc.collect()
            try:
                import torch
                if torch.cuda.is_available():
                    torch.cuda.empty_cache()
                    memory_allocated = torch.cuda.memory_allocated() / 1024**3
                    logger.info(f"GPU memory after formatter unload: {memory_allocated:.2f} GB")
            except ImportError:
                pass

            logger.info("Formatter model unloaded")

    def is_initialized(self) -> bool:
        """Check if formatter is ready."""
        with self._lock:
            return self._initialized and self.model is not None

    def format_text(self, raw_text: str) -> str:
        """
        Format transcribed text using the LLM.

        Args:
            raw_text: Raw transcription text

        Returns:
            Formatted text, or original text if formatting fails
        """
        if not raw_text or not raw_text.strip():
            return raw_text

        with self._lock:
            if not self.is_initialized():
                logger.debug("Formatter not initialized, returning raw text")
                return raw_text

            try:
                prompt = FORMATTING_PROMPT.format(text=raw_text)

                output = self.model(
                    prompt,
                    max_tokens=512,
                    stop=["<|im_end|>", "<|im_start|>"],
                    temperature=0.1,
                    echo=False
                )

                formatted = output["choices"][0]["text"].strip()

                # Sanity check - if response is empty or way longer, use original
                if not formatted or len(formatted) > len(raw_text) * 3:
                    logger.warning("Formatting produced unexpected result, using raw text")
                    return raw_text

                logger.debug(f"Formatted: '{raw_text[:30]}...' -> '{formatted[:30]}...'")
                return formatted

            except Exception as e:
                logger.warning(f"Text formatting failed, using raw text: {e}")
                return raw_text


# Global formatter instance (lazy loaded)
formatter = LocalLLMFormatter()
