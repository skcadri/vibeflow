"""Transcription history tab."""

import logging
from datetime import datetime
from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QTableWidget,
    QTableWidgetItem, QPushButton, QLabel, QHeaderView,
    QAbstractItemView, QMessageBox, QApplication
)

logger = logging.getLogger(__name__)


class HistoryTab(QWidget):
    """Tab for viewing transcription history."""

    def __init__(self, app, parent=None):
        super().__init__(parent)
        self.app = app
        self._setup_ui()
        self.load_history()

    def _setup_ui(self):
        """Create UI components."""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(20, 20, 20, 20)
        layout.setSpacing(16)

        # Header with title and actions
        header = QHBoxLayout()

        title = QLabel("Transcription History")
        title.setObjectName("sectionTitle")
        header.addWidget(title)

        header.addStretch()

        refresh_btn = QPushButton("Refresh")
        refresh_btn.clicked.connect(self.load_history)
        header.addWidget(refresh_btn)

        clear_btn = QPushButton("Clear History")
        clear_btn.setObjectName("dangerButton")
        clear_btn.clicked.connect(self._clear_history)
        header.addWidget(clear_btn)

        layout.addLayout(header)

        # History table
        self.table = QTableWidget()
        self.table.setColumnCount(4)
        self.table.setHorizontalHeaderLabels(["Time", "Text", "Model", "Duration"])
        self.table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.table.setAlternatingRowColors(True)

        # Connect click event to copy text to clipboard
        self.table.cellClicked.connect(self._on_cell_clicked)

        # Column sizing
        header = self.table.horizontalHeader()
        header.setSectionResizeMode(0, QHeaderView.ResizeMode.ResizeToContents)
        header.setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)
        header.setSectionResizeMode(2, QHeaderView.ResizeMode.ResizeToContents)
        header.setSectionResizeMode(3, QHeaderView.ResizeMode.ResizeToContents)

        layout.addWidget(self.table)

        # Hint label
        hint = QLabel("Click any row to copy the transcribed text to clipboard")
        hint.setObjectName("description")
        layout.addWidget(hint)

        # Status bar
        self.status_label = QLabel("0 entries")
        self.status_label.setObjectName("description")
        layout.addWidget(self.status_label)

    def _on_cell_clicked(self, row, column):
        """Handle cell click - copy text to clipboard."""
        # Get the text from column 1 (Text column)
        text_item = self.table.item(row, 1)
        if text_item:
            text = text_item.text()
            clipboard = QApplication.clipboard()
            clipboard.setText(text)
            logger.info(f"Copied to clipboard: {text[:50]}...")
            # Update status temporarily to show feedback
            old_status = self.status_label.text()
            self.status_label.setText("✓ Copied to clipboard!")
            # Reset after 2 seconds
            from PyQt6.QtCore import QTimer
            QTimer.singleShot(2000, lambda: self.status_label.setText(old_status))

    def load_history(self):
        """Load history from storage."""
        self.table.setRowCount(0)

        if hasattr(self.app, 'history_storage'):
            entries = self.app.history_storage.get_recent(limit=500)

            for entry in entries:
                row = self.table.rowCount()
                self.table.insertRow(row)

                # Format timestamp
                timestamp = datetime.fromisoformat(entry['timestamp'])
                time_str = timestamp.strftime("%Y-%m-%d %H:%M")

                self.table.setItem(row, 0, QTableWidgetItem(time_str))
                self.table.setItem(row, 1, QTableWidgetItem(entry['text']))
                self.table.setItem(row, 2, QTableWidgetItem(entry['model']))
                self.table.setItem(row, 3, QTableWidgetItem(f"{entry['duration']:.1f}s"))

        self._update_status()

    def _clear_history(self):
        """Clear all history."""
        reply = QMessageBox.question(
            self, "Clear History",
            "Are you sure you want to delete all transcription history?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        if reply == QMessageBox.StandardButton.Yes:
            if hasattr(self.app, 'history_storage'):
                self.app.history_storage.clear_all()
            self.table.setRowCount(0)
            self._update_status()

    def _update_status(self):
        """Update status label."""
        count = self.table.rowCount()
        self.status_label.setText(f"{count} entr{'ies' if count != 1 else 'y'}")
