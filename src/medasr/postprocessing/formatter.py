"""Local LLM text formatter using Phi-3-mini for post-processing."""

import gc
import logging
import threading
from typing import Optional

from ..config import config

logger = logging.getLogger(__name__)

# Default formatting prompt - strict mode (preserves original words exactly)
DEFAULT_PROMPT_STRICT = """<|user|>
You are a text formatter. Your ONLY job is to add line breaks and bullet points.

CRITICAL RULES:
1. DO NOT change any words
2. DO NOT add or remove any words
3. DO NOT rephrase anything
4. ONLY add line breaks between sentences when appropriate
5. ONLY convert comma-separated lists to bullet points
6. Keep ALL original words exactly as they are

Example 1:
Input: Hi, how are you? I wanted to ask about the meeting. Thanks. John.
Output:
Hi, how are you?

I wanted to ask about the meeting.

Thanks.

John.

Example 2:
Input: I need milk, eggs, bread, and butter.
Output:
I need:
- milk
- eggs
- bread
- butter

Now format this text (preserve ALL original words):
{text}<|end|>
<|assistant|>
"""

# Default formatting prompt - typo fix mode (can fix obvious typos)
DEFAULT_PROMPT_TYPOFIX = """<|user|>
You are a text formatter. Add line breaks, bullet points, and fix obvious typos.

RULES:
1. Add line breaks between sentences when appropriate
2. Convert comma-separated lists to bullet points
3. Fix obvious spelling mistakes and typos
4. DO NOT rephrase or change the meaning
5. DO NOT add new words or remove words (except fixing typos)

Example 1:
Input: Hi, how are yuo? I wantd to ask about the meetting. Thanks. John.
Output:
Hi, how are you?

I wanted to ask about the meeting.

Thanks.

John.

Example 2:
Input: I need milk, egss, bread, and buttr.
Output:
I need:
- milk
- eggs
- bread
- butter

Now format this text (fix typos, add line breaks):
{text}<|end|>
<|assistant|>
"""


class LocalLLMFormatter:
    """Formats transcribed text using a local Phi-3-mini model."""

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
                logger.info("Loading Phi-3-mini formatter model...")
                from llama_cpp import Llama

                self.model = Llama.from_pretrained(
                    repo_id="microsoft/Phi-3-mini-4k-instruct-gguf",
                    filename="Phi-3-mini-4k-instruct-q4.gguf",
                    n_ctx=1024,
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

    def format_text(self, raw_text: str, fix_typos: bool = False) -> str:
        """
        Format transcribed text using the LLM.

        Args:
            raw_text: Raw transcription text
            fix_typos: If True, also fix obvious typos

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
                # Get prompt template - use custom from config if available
                if fix_typos:
                    prompt_template = config.get('formatting.prompt_typofix', DEFAULT_PROMPT_TYPOFIX)
                else:
                    prompt_template = config.get('formatting.prompt_strict', DEFAULT_PROMPT_STRICT)

                prompt = prompt_template.format(text=raw_text)

                output = self.model(
                    prompt,
                    max_tokens=512,
                    stop=["<|end|>", "<|user|>", "<|assistant|>"],
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
