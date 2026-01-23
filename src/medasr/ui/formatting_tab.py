"""Formatting settings tab for text post-processing configuration."""

import logging
import threading
from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QCheckBox, QFrame
)
from PyQt6.QtCore import pyqtSignal

from ..config import config

logger = logging.getLogger(__name__)


class FormattingTab(QWidget):
    """Tab for configuring text formatting settings."""

    formatting_toggled = pyqtSignal(bool)  # Emits enabled state

    def __init__(self, app, parent=None):
        super().__init__(parent)
        self.app = app
        self._loading = False
        self._setup_ui()
        self._load_settings()

    def _setup_ui(self):
        """Create UI components."""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(20, 20, 20, 20)
        layout.setSpacing(16)

        # Title and description
        title = QLabel("Text Formatting")
        title.setObjectName("sectionTitle")
        layout.addWidget(title)

        desc = QLabel(
            "Use a local AI model (Phi-3-mini) to format transcribed text. "
            "Adds line breaks between sentences and converts comma-separated lists to bullet points. "
            "The model preserves your original words exactly."
        )
        desc.setObjectName("description")
        desc.setWordWrap(True)
        layout.addWidget(desc)

        # Enable toggle section
        toggle_frame = QFrame()
        toggle_frame.setObjectName("card")
        toggle_layout = QVBoxLayout(toggle_frame)
        toggle_layout.setSpacing(12)

        # Enable checkbox
        self.enable_checkbox = QCheckBox("Enable text formatting")
        self.enable_checkbox.stateChanged.connect(self._on_enable_changed)
        toggle_layout.addWidget(self.enable_checkbox)

        # Fix typos checkbox
        self.fix_typos_checkbox = QCheckBox("Also fix obvious typos")
        self.fix_typos_checkbox.setEnabled(False)  # Disabled until formatting is enabled
        self.fix_typos_checkbox.stateChanged.connect(self._on_fix_typos_changed)
        toggle_layout.addWidget(self.fix_typos_checkbox)

        # Status label
        self.status_label = QLabel("")
        self.status_label.setObjectName("description")
        toggle_layout.addWidget(self.status_label)

        layout.addWidget(toggle_frame)

        # Info section
        info_frame = QFrame()
        info_frame.setObjectName("card")
        info_layout = QVBoxLayout(info_frame)

        info_title = QLabel("How it works")
        info_title.setObjectName("sectionTitle")
        info_layout.addWidget(info_title)

        info_text = QLabel(
            "When enabled, a local AI model (~2.5GB VRAM) formats your transcriptions "
            "before pasting them. The model is unloaded from memory when disabled.\n\n"
            "Examples:\n"
            "- 'I need milk, eggs, bread' → bullet list\n"
            "- 'Hi how are you. I wanted to ask...' → proper paragraph breaks\n\n"
            "With 'Fix typos' enabled, obvious spelling mistakes are also corrected."
        )
        info_text.setObjectName("description")
        info_text.setWordWrap(True)
        info_layout.addWidget(info_text)

        layout.addWidget(info_frame)

        # Add stretch to push everything to top
        layout.addStretch()

        # Note about first-time download
        note_label = QLabel(
            "Note: The model will be downloaded automatically on first enable (~2.5GB)."
        )
        note_label.setObjectName("description")
        note_label.setWordWrap(True)
        layout.addWidget(note_label)

    def _load_settings(self):
        """Load current settings."""
        enabled = config.get('formatting.enabled', False)
        fix_typos = config.get('formatting.fix_typos', False)
        self.enable_checkbox.setChecked(enabled)
        self.fix_typos_checkbox.setChecked(fix_typos)
        self.fix_typos_checkbox.setEnabled(enabled)
        self._update_status()

    def _on_enable_changed(self, state):
        """Handle enable checkbox change."""
        enabled = state == 2  # Qt.CheckState.Checked

        # Save to config
        config.set('formatting.enabled', enabled)
        config.save()

        # Enable/disable fix typos checkbox
        self.fix_typos_checkbox.setEnabled(enabled)

        if enabled:
            self._load_model()
        else:
            self._unload_model()

        self.formatting_toggled.emit(enabled)

    def _on_fix_typos_changed(self, state):
        """Handle fix typos checkbox change."""
        fix_typos = state == 2  # Qt.CheckState.Checked
        config.set('formatting.fix_typos', fix_typos)
        config.save()
        logger.info(f"Fix typos {'enabled' if fix_typos else 'disabled'}")

    def _load_model(self):
        """Load the formatter model in background."""
        if self._loading:
            return

        self._loading = True
        self.status_label.setText("Loading formatter model...")
        self.enable_checkbox.setEnabled(False)

        def load():
            try:
                if hasattr(self.app, 'formatter') and self.app.formatter:
                    success = self.app.formatter.initialize()
                    if success:
                        self._on_model_loaded(True)
                    else:
                        self._on_model_loaded(False)
                else:
                    self._on_model_loaded(False)
            except Exception as e:
                logger.error(f"Failed to load formatter: {e}")
                self._on_model_loaded(False)

        thread = threading.Thread(target=load, daemon=True)
        thread.start()

    def _on_model_loaded(self, success: bool):
        """Handle model load completion (called from background thread)."""
        # Use Qt signal/slot or direct update since we're on background thread
        # For simplicity, just update directly (may need QMetaObject.invokeMethod for thread safety)
        self._loading = False
        self.enable_checkbox.setEnabled(True)

        if success:
            self.status_label.setText("Formatting enabled and ready")
        else:
            self.status_label.setText("Failed to load model - formatting disabled")
            self.enable_checkbox.setChecked(False)
            config.set('formatting.enabled', False)
            config.save()

    def _unload_model(self):
        """Unload the formatter model."""
        self.status_label.setText("Unloading formatter model...")

        if hasattr(self.app, 'formatter') and self.app.formatter:
            self.app.formatter.unload()

        self.status_label.setText("Formatting disabled")

    def _update_status(self):
        """Update status label based on current state."""
        enabled = config.get('formatting.enabled', False)

        if not enabled:
            self.status_label.setText("Formatting is disabled")
        elif hasattr(self.app, 'formatter') and self.app.formatter and self.app.formatter.is_initialized():
            self.status_label.setText("Formatting enabled and ready")
        else:
            self.status_label.setText("Formatting enabled (model not loaded)")
