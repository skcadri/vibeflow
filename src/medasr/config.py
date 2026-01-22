"""Configuration management for MedASR."""

import logging
from pathlib import Path
from typing import Optional
import yaml

logger = logging.getLogger(__name__)


class Config:
    """Application configuration."""

    def __init__(self, config_path: Optional[Path] = None):
        """Load configuration from YAML file."""
        if config_path is None:
            # Default to config/settings.yaml relative to project root
            config_path = Path(__file__).parent.parent.parent / "config" / "settings.yaml"

        self.config_path = config_path
        self._data = {}
        self.load()

    def load(self):
        """Load configuration from file."""
        if not self.config_path.exists():
            logger.warning(f"Config file not found: {self.config_path}")
            self._data = self._default_config()
            return

        try:
            with open(self.config_path, 'r') as f:
                self._data = yaml.safe_load(f)
            logger.info(f"Loaded config from: {self.config_path}")
        except Exception as e:
            logger.error(f"Failed to load config: {e}")
            self._data = self._default_config()

    def _default_config(self) -> dict:
        """Default configuration values."""
        return {
            'transcription': {
                'model': 'google/medasr',
                'device': 'cuda'
            },
            'audio': {
                'sample_rate': 16000,
                'channels': 1,
                'chunk_size': 1024
            },
            'hotkeys': {
                'toggle': 'ctrl+cmd',
                'cancel': 'escape'
            },
            'ui': {
                'show_bubble': True,
                'bubble_position': 'bottom_center',
                'play_sounds': True,
                'waveform_bars': 20
            },
            'history': {
                'enabled': True,
                'max_entries': 500
            }
        }

    def get(self, key: str, default=None):
        """Get config value by dot-notation key (e.g., 'audio.sample_rate')."""
        keys = key.split('.')
        value = self._data
        for k in keys:
            if isinstance(value, dict):
                value = value.get(k)
            else:
                return default
            if value is None:
                return default
        return value


# Global config instance
config = Config()
