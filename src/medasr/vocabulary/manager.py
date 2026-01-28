"""Vocabulary/hotwords manager for faster-whisper."""

import logging
from pathlib import Path
from typing import List, Optional

from ..config import config

logger = logging.getLogger(__name__)


class VocabularyManager:
    """Manages custom vocabulary words and vocabulary packs for hotwords feature."""

    def __init__(self, vocab_path: Optional[Path] = None, packs_dir: Optional[Path] = None):
        """
        Initialize vocabulary manager.

        Args:
            vocab_path: Path to custom vocabulary file
            packs_dir: Path to vocabulary packs directory
        """
        if vocab_path is None:
            vocab_path = Path(__file__).parent.parent.parent.parent / "config" / "vocabulary.txt"
        if packs_dir is None:
            packs_dir = Path(__file__).parent.parent.parent.parent / "config" / "vocabulary"

        self.vocab_path = vocab_path
        self.packs_dir = packs_dir
        self._words: List[str] = []
        self._load()

    def _load(self):
        """Load custom vocabulary from file."""
        if not self.vocab_path.exists():
            self._words = []
            logger.info(f"No vocabulary file found at {self.vocab_path}")
            return

        try:
            with open(self.vocab_path, 'r', encoding='utf-8') as f:
                self._words = [line.strip() for line in f if line.strip()]
            logger.info(f"Loaded {len(self._words)} custom vocabulary words")
        except Exception as e:
            logger.error(f"Failed to load vocabulary: {e}")
            self._words = []

    def get_words(self) -> List[str]:
        """Get list of custom vocabulary words (not including packs)."""
        return self._words.copy()

    def get_available_packs(self) -> List[str]:
        """
        Get list of available vocabulary pack names.

        Returns:
            Sorted list of pack names (filename stems) found in the packs directory.
        """
        if not self.packs_dir.exists():
            return []

        packs = []
        for f in sorted(self.packs_dir.iterdir()):
            if f.suffix == '.txt' and f.is_file():
                packs.append(f.stem)
        return packs

    def get_enabled_packs(self) -> List[str]:
        """Get list of enabled pack names from config."""
        return config.get('vocabulary.enabled_packs', []) or []

    def set_enabled_packs(self, packs: List[str]):
        """Save enabled packs list to config."""
        config.set('vocabulary.enabled_packs', packs)
        config.save()
        logger.info(f"Enabled vocabulary packs: {packs}")

    def get_pack_words(self, pack_name: str) -> List[str]:
        """
        Load words from a specific pack file.

        Args:
            pack_name: Pack name (without .txt extension)

        Returns:
            List of words in the pack.
        """
        pack_path = self.packs_dir / f"{pack_name}.txt"
        if not pack_path.exists():
            logger.warning(f"Pack file not found: {pack_path}")
            return []

        try:
            with open(pack_path, 'r', encoding='utf-8') as f:
                return [line.strip() for line in f if line.strip()]
        except Exception as e:
            logger.error(f"Failed to load pack '{pack_name}': {e}")
            return []

    def get_pack_word_count(self, pack_name: str) -> int:
        """Get word count for a pack without loading all words into a list."""
        pack_path = self.packs_dir / f"{pack_name}.txt"
        if not pack_path.exists():
            return 0
        try:
            with open(pack_path, 'r', encoding='utf-8') as f:
                return sum(1 for line in f if line.strip())
        except Exception:
            return 0

    def get_all_hotwords(self) -> List[str]:
        """
        Get all hotwords from enabled packs + custom words, deduplicated.

        Returns:
            Combined list of unique words (case-insensitive dedup).
        """
        all_words = []
        seen = set()

        # Add words from enabled packs
        for pack_name in self.get_enabled_packs():
            for word in self.get_pack_words(pack_name):
                lower = word.lower()
                if lower not in seen:
                    seen.add(lower)
                    all_words.append(word)

        # Add custom words
        for word in self._words:
            lower = word.lower()
            if lower not in seen:
                seen.add(lower)
                all_words.append(word)

        return all_words

    def get_hotwords_string(self) -> Optional[str]:
        """
        Get all vocabulary (packs + custom) as hotwords string for faster-whisper.

        Returns:
            Comma-separated string of words, or None if empty
        """
        all_words = self.get_all_hotwords()
        if not all_words:
            return None
        return ", ".join(all_words)

    def save_words(self, words: List[str]):
        """
        Save custom vocabulary words to file.

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
            logger.info(f"Saved {len(words)} custom vocabulary words")
        except Exception as e:
            logger.error(f"Failed to save vocabulary: {e}")

    def add_word(self, word: str) -> bool:
        """Add a word to custom vocabulary."""
        if word in self._words:
            return False
        self._words.append(word)
        self.save_words(self._words)
        return True

    def remove_word(self, word: str) -> bool:
        """Remove a word from custom vocabulary."""
        if word not in self._words:
            return False
        self._words.remove(word)
        self.save_words(self._words)
        return True
