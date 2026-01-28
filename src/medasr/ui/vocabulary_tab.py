"""Vocabulary management tab for custom words/hotwords and vocabulary packs."""

import logging
from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QListWidget,
    QLineEdit, QPushButton, QLabel, QCheckBox,
    QMessageBox, QFrame
)
from PyQt6.QtCore import pyqtSignal, Qt

logger = logging.getLogger(__name__)


class VocabularyTab(QWidget):
    """Tab for managing vocabulary packs and custom words."""

    vocabulary_updated = pyqtSignal(list)  # Emits updated word list

    def __init__(self, app, parent=None):
        super().__init__(parent)
        self.app = app
        self._pack_checkboxes = {}
        self._setup_ui()
        self._load_vocabulary()

    def _setup_ui(self):
        """Create UI components."""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(20, 20, 20, 20)
        layout.setSpacing(16)

        # ── Vocabulary Packs Section ──
        packs_title = QLabel("Vocabulary Packs")
        packs_title.setObjectName("sectionTitle")
        layout.addWidget(packs_title)

        packs_desc = QLabel(
            "Enable specialty packs to improve recognition of domain-specific terms. "
            "Words from enabled packs are combined with your custom vocabulary."
        )
        packs_desc.setObjectName("description")
        packs_desc.setWordWrap(True)
        layout.addWidget(packs_desc)

        # Packs card
        packs_frame = QFrame()
        packs_frame.setObjectName("card")
        self._packs_layout = QVBoxLayout(packs_frame)
        self._packs_layout.setSpacing(8)

        layout.addWidget(packs_frame)

        # ── Custom Words Section ──
        title = QLabel("Custom Words")
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

        # Word count (combined)
        self.count_label = QLabel("0 words")
        self.count_label.setObjectName("description")
        layout.addWidget(self.count_label)

    # ── Vocabulary Packs ──

    def _load_packs(self):
        """Populate the packs section with checkboxes."""
        if not hasattr(self.app, 'vocabulary_manager'):
            return

        vm = self.app.vocabulary_manager
        available = vm.get_available_packs()
        enabled = vm.get_enabled_packs()

        # Clear existing checkboxes
        for cb in self._pack_checkboxes.values():
            self._packs_layout.removeWidget(cb)
            cb.deleteLater()
        self._pack_checkboxes.clear()

        if not available:
            no_packs = QLabel("No vocabulary packs found in config/vocabulary/")
            no_packs.setObjectName("description")
            self._packs_layout.addWidget(no_packs)
            return

        for pack_name in available:
            word_count = vm.get_pack_word_count(pack_name)
            display_name = pack_name.replace('_', ' ').title()
            label = f"{display_name} ({word_count} terms)"

            cb = QCheckBox(label)
            cb.setChecked(pack_name in enabled)
            cb.setProperty('pack_name', pack_name)
            cb.stateChanged.connect(self._on_pack_toggled)
            self._packs_layout.addWidget(cb)
            self._pack_checkboxes[pack_name] = cb

    def _on_pack_toggled(self, state):
        """Handle pack checkbox toggle."""
        if not hasattr(self.app, 'vocabulary_manager'):
            return

        enabled = [
            name for name, cb in self._pack_checkboxes.items()
            if cb.isChecked()
        ]
        self.app.vocabulary_manager.set_enabled_packs(enabled)
        self._update_count()

        # Emit all hotwords (packs + custom)
        all_words = self.app.vocabulary_manager.get_all_hotwords()
        self.vocabulary_updated.emit(all_words)

    # ── Edit handling (crash fix: blockSignals around flag changes) ──

    def _on_item_double_clicked(self, item):
        """Handle double-click on item to edit."""
        self.word_list.blockSignals(True)
        try:
            item.setFlags(item.flags() | Qt.ItemFlag.ItemIsEditable)
        finally:
            self.word_list.blockSignals(False)
        self.word_list.editItem(item)

    def _on_item_changed(self, item):
        """Handle item text change after editing."""
        self.word_list.blockSignals(True)
        try:
            item.setFlags(item.flags() & ~Qt.ItemFlag.ItemIsEditable)
        finally:
            self.word_list.blockSignals(False)
        self._save_and_emit()

    # ── Load / Save ──

    def _load_vocabulary(self):
        """Load vocabulary packs and custom words from manager."""
        self._load_packs()

        if hasattr(self.app, 'vocabulary_manager'):
            words = self.app.vocabulary_manager.get_words()
            self.word_list.blockSignals(True)
            self.word_list.clear()
            for word in words:
                self.word_list.addItem(word)
            self.word_list.blockSignals(False)
            self._update_count()

    def _add_word(self):
        """Add word to custom vocabulary."""
        word = self.word_input.text().strip()
        if not word:
            return

        # Check for duplicates in custom words
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
        """Clear all custom words."""
        reply = QMessageBox.question(
            self, "Clear Vocabulary",
            "Are you sure you want to remove all custom words?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        if reply == QMessageBox.StandardButton.Yes:
            self.word_list.clear()
            self._save_and_emit()
            self._update_count()

    def _save_and_emit(self):
        """Save custom vocabulary and emit signal with all hotwords."""
        words = [self.word_list.item(i).text()
                for i in range(self.word_list.count())]

        if hasattr(self.app, 'vocabulary_manager'):
            self.app.vocabulary_manager.save_words(words)
            all_words = self.app.vocabulary_manager.get_all_hotwords()
            self.vocabulary_updated.emit(all_words)
        else:
            self.vocabulary_updated.emit(words)

    def _update_count(self):
        """Update word count label showing total (packs + custom)."""
        custom_count = self.word_list.count()

        if hasattr(self.app, 'vocabulary_manager'):
            total = len(self.app.vocabulary_manager.get_all_hotwords())
            pack_count = total - custom_count
            if pack_count > 0:
                self.count_label.setText(
                    f"{total} total words ({custom_count} custom + {pack_count} from packs)"
                )
            else:
                self.count_label.setText(
                    f"{custom_count} custom word{'s' if custom_count != 1 else ''}"
                )
        else:
            self.count_label.setText(
                f"{custom_count} word{'s' if custom_count != 1 else ''}"
            )
