"""Vocabulary/hotwords manager for faster-whisper."""

import logging
from pathlib import Path
from typing import List, Optional

logger = logging.getLogger(__name__)


class VocabularyManager:
    """Manages custom vocabulary words for hotwords feature."""

    def __init__(self, vocab_path: Optional[Path] = None):
        """
        Initialize vocabulary manager.

        Args:
            vocab_path: Path to vocabulary file
        """
        if vocab_path is None:
            vocab_path = Path(__file__).parent.parent.parent.parent / "config" / "vocabulary.txt"

        self.vocab_path = vocab_path
        self._words: List[str] = []
        self._load()

    def _load(self):
        """Load vocabulary from file."""
        if not self.vocab_path.exists():
            self._words = []
            logger.info(f"No vocabulary file found at {self.vocab_path}")
            return

        try:
            with open(self.vocab_path, 'r', encoding='utf-8') as f:
                self._words = [line.strip() for line in f if line.strip()]
            logger.info(f"Loaded {len(self._words)} vocabulary words")
        except Exception as e:
            logger.error(f"Failed to load vocabulary: {e}")
            self._words = []

    def get_words(self) -> List[str]:
        """Get list of vocabulary words."""
        return self._words.copy()

    def get_hotwords_string(self) -> Optional[str]:
        """
        Get vocabulary as hotwords string for faster-whisper.

        Returns:
            Comma-separated string of words, or None if empty
        """
        if not self._words:
            return None
        return ", ".join(self._words)

    def save_words(self, words: List[str]):
        """
        Save vocabulary words to file.

        Args:
            words: List of words to save
        """
        self._words = words

        # Ensure directory exists
        self.vocab_path.parent.mkdir(parents=True, exist_ok=True)

        try:
            with open(self.vocab_path, 'w', encoding='utf-8') as f:
                for word in words:
                    f.write(word + '\n')
            logger.info(f"Saved {len(words)} vocabulary words")
        except Exception as e:
            logger.error(f"Failed to save vocabulary: {e}")

    def add_word(self, word: str) -> bool:
        """Add a word to vocabulary."""
        if word in self._words:
            return False
        self._words.append(word)
        self.save_words(self._words)
        return True

    def remove_word(self, word: str) -> bool:
        """Remove a word from vocabulary."""
        if word not in self._words:
            return False
        self._words.remove(word)
        self.save_words(self._words)
        return True
