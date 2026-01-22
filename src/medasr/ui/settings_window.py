"""Settings window with Vocabulary, Models, and History tabs."""

import logging
from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QTabWidget,
    QLabel
)
from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QFont

from .styles import DARK_THEME_QSS
from .vocabulary_tab import VocabularyTab
from .models_tab import ModelsTab
from .history_tab import HistoryTab

logger = logging.getLogger(__name__)


class SettingsWindow(QWidget):
    """Main settings window with tabbed interface."""

    # Signals to communicate with main app
    model_changed = pyqtSignal(str)          # model_key
    vocabulary_changed = pyqtSignal(list)     # list of words

    def __init__(self, app, parent=None):
        super().__init__(parent)
        self.app = app
        self._setup_window()
        self._setup_ui()
        self._apply_styles()

    def _setup_window(self):
        """Configure window properties."""
        self.setWindowTitle("MedASR Settings")
        self.setWindowFlags(
            Qt.WindowType.Window |
            Qt.WindowType.WindowCloseButtonHint |
            Qt.WindowType.WindowMinimizeButtonHint
        )
        self.setMinimumSize(600, 500)
        self.resize(700, 550)

    def _setup_ui(self):
        """Create UI components."""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        # Header
        header = self._create_header()
        layout.addWidget(header)

        # Tab widget
        self.tabs = QTabWidget()
        self.tabs.setDocumentMode(True)

        # Create tabs
        self.vocabulary_tab = VocabularyTab(self.app)
        self.models_tab = ModelsTab(self.app)
        self.history_tab = HistoryTab(self.app)

        self.tabs.addTab(self.vocabulary_tab, "Vocabulary")
        self.tabs.addTab(self.models_tab, "Models")
        self.tabs.addTab(self.history_tab, "History")

        layout.addWidget(self.tabs)

        # Connect signals
        self.models_tab.model_selected.connect(self.model_changed.emit)
        self.vocabulary_tab.vocabulary_updated.connect(self.vocabulary_changed.emit)

    def _create_header(self) -> QWidget:
        """Create header with title."""
        header = QWidget()
        header.setObjectName("header")
        header.setFixedHeight(60)

        layout = QHBoxLayout(header)
        layout.setContentsMargins(20, 0, 20, 0)

        title = QLabel("MedASR Settings")
        title.setObjectName("headerTitle")
        font = QFont()
        font.setPointSize(16)
        font.setWeight(QFont.Weight.DemiBold)
        title.setFont(font)

        layout.addWidget(title)
        layout.addStretch()

        return header

    def _apply_styles(self):
        """Apply dark theme styling."""
        self.setStyleSheet(DARK_THEME_QSS)

    def show_and_focus(self):
        """Show window and bring to front."""
        self.show()
        self.raise_()
        self.activateWindow()

    def refresh_history(self):
        """Refresh history tab data."""
        self.history_tab.load_history()
