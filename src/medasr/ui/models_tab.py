"""Model selection tab with descriptions."""

import logging
from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QScrollArea, QFrame,
    QRadioButton, QLabel, QButtonGroup
)
from PyQt6.QtCore import pyqtSignal

logger = logging.getLogger(__name__)

# Model definitions with descriptions
MODEL_INFO = {
    'whisper_tiny': {
        'name': 'Whisper Tiny',
        'description': 'Fastest model, ~39M parameters. Best for quick, basic transcription.',
        'speed': 'Very Fast',
        'accuracy': 'Basic',
        'vram': '~1 GB'
    },
    'whisper_base': {
        'name': 'Whisper Base (Recommended)',
        'description': 'Good balance of speed and accuracy. ~74M parameters.',
        'speed': 'Fast',
        'accuracy': 'Good',
        'vram': '~1 GB'
    },
    'whisper_small': {
        'name': 'Whisper Small',
        'description': 'Better accuracy than Base. ~244M parameters.',
        'speed': 'Medium',
        'accuracy': 'Better',
        'vram': '~2 GB'
    },
    'whisper_medium': {
        'name': 'Whisper Medium',
        'description': 'High accuracy for most use cases. ~769M parameters.',
        'speed': 'Slower',
        'accuracy': 'High',
        'vram': '~5 GB'
    },
    'whisper_large': {
        'name': 'Whisper Large-v3',
        'description': 'Best accuracy, multilingual. ~1.5B parameters.',
        'speed': 'Slow',
        'accuracy': 'Best',
        'vram': '~10 GB'
    },
    'whisper_turbo': {
        'name': 'Whisper Large-v3-Turbo',
        'description': 'Best accuracy with improved speed. Optimized Large-v3.',
        'speed': 'Medium',
        'accuracy': 'Best',
        'vram': '~6 GB'
    },
    'distil_whisper': {
        'name': 'Distil-Whisper Large',
        'description': 'Speed champion with near-Large accuracy. Distilled model.',
        'speed': 'Fast',
        'accuracy': 'High',
        'vram': '~4 GB'
    },
    'medasr': {
        'name': 'MedASR (Medical)',
        'description': 'Specialized for medical dictation. Best for clinical terminology.',
        'speed': 'Medium',
        'accuracy': 'Medical',
        'vram': '~4 GB'
    }
}


class ModelsTab(QWidget):
    """Tab for selecting transcription model."""

    model_selected = pyqtSignal(str)  # Emits model key

    def __init__(self, app, parent=None):
        super().__init__(parent)
        self.app = app
        self.button_group = QButtonGroup(self)
        self._setup_ui()

    def _setup_ui(self):
        """Create UI components."""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(20, 20, 20, 20)
        layout.setSpacing(16)

        # Title
        title = QLabel("Transcription Model")
        title.setObjectName("sectionTitle")
        layout.addWidget(title)

        desc = QLabel(
            "Select a model based on your needs. Larger models are more accurate "
            "but require more GPU memory and are slower."
        )
        desc.setObjectName("description")
        desc.setWordWrap(True)
        layout.addWidget(desc)

        # Scroll area for model cards
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)

        scroll_content = QWidget()
        scroll_layout = QVBoxLayout(scroll_content)
        scroll_layout.setSpacing(12)

        current_model = getattr(self.app, 'current_model', 'whisper_base')

        for model_key, info in MODEL_INFO.items():
            card = self._create_model_card(model_key, info, model_key == current_model)
            scroll_layout.addWidget(card)

        scroll_layout.addStretch()
        scroll.setWidget(scroll_content)
        layout.addWidget(scroll)

        self.button_group.buttonClicked.connect(self._on_model_selected)

    def _create_model_card(self, model_key: str, info: dict, is_selected: bool) -> QFrame:
        """Create a model selection card."""
        card = QFrame()
        card.setObjectName("card")

        layout = QVBoxLayout(card)
        layout.setSpacing(8)

        # Radio button with model name
        radio = QRadioButton(info['name'])
        radio.setChecked(is_selected)
        radio.setProperty('model_key', model_key)
        self.button_group.addButton(radio)
        layout.addWidget(radio)

        # Description
        desc = QLabel(info['description'])
        desc.setObjectName("description")
        desc.setWordWrap(True)
        layout.addWidget(desc)

        # Stats row
        stats = QLabel(f"Speed: {info['speed']} | Accuracy: {info['accuracy']} | VRAM: {info['vram']}")
        stats.setObjectName("description")
        stats.setStyleSheet("color: #666666; font-size: 11px;")
        layout.addWidget(stats)

        return card

    def _on_model_selected(self, button):
        """Handle model selection."""
        model_key = button.property('model_key')
        if model_key:
            logger.info(f"Model selected: {model_key}")
            self.model_selected.emit(model_key)
