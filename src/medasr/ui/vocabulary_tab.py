"""Vocabulary management tab for custom words/hotwords."""

import logging
from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QListWidget,
    QLineEdit, QPushButton, QLabel,
    QMessageBox
)
from PyQt6.QtCore import pyqtSignal, Qt

logger = logging.getLogger(__name__)


class VocabularyTab(QWidget):
    """Tab for managing custom vocabulary words."""

    vocabulary_updated = pyqtSignal(list)  # Emits updated word list

    def __init__(self, app, parent=None):
        super().__init__(parent)
        self.app = app
        self._setup_ui()
        self._load_vocabulary()

    def _setup_ui(self):
        """Create UI components."""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(20, 20, 20, 20)
        layout.setSpacing(16)

        # Title and description
        title = QLabel("Custom Vocabulary")
        title.setObjectName("sectionTitle")
        layout.addWidget(title)

        desc = QLabel(
            "Add medical terms, names, or specialized words to improve recognition accuracy. "
            "These words will be prioritized during transcription."
        )
        desc.setObjectName("description")
        desc.setWordWrap(True)
        layout.addWidget(desc)

        # Add word input
        input_layout = QHBoxLayout()
        input_layout.setSpacing(10)

        self.word_input = QLineEdit()
        self.word_input.setPlaceholderText("Enter a word or phrase...")
        self.word_input.returnPressed.connect(self._add_word)
        input_layout.addWidget(self.word_input)

        add_btn = QPushButton("Add")
        add_btn.setObjectName("primaryButton")
        add_btn.setFixedWidth(80)
        add_btn.clicked.connect(self._add_word)
        input_layout.addWidget(add_btn)

        layout.addLayout(input_layout)

        # Word list
        self.word_list = QListWidget()
        self.word_list.setSelectionMode(QListWidget.SelectionMode.ExtendedSelection)
        # Enable double-click to edit
        self.word_list.itemDoubleClicked.connect(self._on_item_double_clicked)
        self.word_list.itemChanged.connect(self._on_item_changed)
        layout.addWidget(self.word_list)

        # Action buttons
        btn_layout = QHBoxLayout()
        btn_layout.setSpacing(10)

        remove_btn = QPushButton("Remove Selected")
        remove_btn.setObjectName("dangerButton")
        remove_btn.clicked.connect(self._remove_selected)
        btn_layout.addWidget(remove_btn)

        btn_layout.addStretch()

        clear_btn = QPushButton("Clear All")
        clear_btn.clicked.connect(self._clear_all)
        btn_layout.addWidget(clear_btn)

        layout.addLayout(btn_layout)

        # Hints
        hint = QLabel("Double-click any word to edit it")
        hint.setObjectName("description")
        layout.addWidget(hint)

        # Word count
        self.count_label = QLabel("0 words")
        self.count_label.setObjectName("description")
        layout.addWidget(self.count_label)

    def _on_item_double_clicked(self, item):
        """Handle double-click on item to edit."""
        item.setFlags(item.flags() | Qt.ItemFlag.ItemIsEditable)
        self.word_list.editItem(item)

    def _on_item_changed(self, item):
        """Handle item text change after editing."""
        # Make item non-editable again
        item.setFlags(item.flags() & ~Qt.ItemFlag.ItemIsEditable)
        # Save the updated vocabulary
        self._save_and_emit()

    def _load_vocabulary(self):
        """Load vocabulary from manager."""
        if hasattr(self.app, 'vocabulary_manager'):
            words = self.app.vocabulary_manager.get_words()
            self.word_list.clear()
            for word in words:
                self.word_list.addItem(word)
            self._update_count()

    def _add_word(self):
        """Add word to vocabulary."""
        word = self.word_input.text().strip()
        if not word:
            return

        # Check for duplicates
        existing = [self.word_list.item(i).text()
                   for i in range(self.word_list.count())]
        if word.lower() in [w.lower() for w in existing]:
            QMessageBox.warning(self, "Duplicate", f"'{word}' is already in the vocabulary.")
            return

        self.word_list.addItem(word)
        self.word_input.clear()
        self._save_and_emit()
        self._update_count()

    def _remove_selected(self):
        """Remove selected words."""
        for item in self.word_list.selectedItems():
            self.word_list.takeItem(self.word_list.row(item))
        self._save_and_emit()
        self._update_count()

    def _clear_all(self):
        """Clear all words."""
        reply = QMessageBox.question(
            self, "Clear Vocabulary",
            "Are you sure you want to remove all words?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        if reply == QMessageBox.StandardButton.Yes:
            self.word_list.clear()
            self._save_and_emit()
            self._update_count()

    def _save_and_emit(self):
        """Save vocabulary and emit signal."""
        words = [self.word_list.item(i).text()
                for i in range(self.word_list.count())]

        if hasattr(self.app, 'vocabulary_manager'):
            self.app.vocabulary_manager.save_words(words)

        self.vocabulary_updated.emit(words)

    def _update_count(self):
        """Update word count label."""
        count = self.word_list.count()
        self.count_label.setText(f"{count} word{'s' if count != 1 else ''}")
