"""Dark theme QSS styles for MedASR settings window."""

DARK_THEME_QSS = """
/* Main Window */
QWidget {
    background-color: #1a1a1a;
    color: #e0e0e0;
    font-family: "Segoe UI", Arial, sans-serif;
    font-size: 13px;
}

/* Header */
QWidget#header {
    background-color: #252525;
    border-bottom: 1px solid #333333;
}

QLabel#headerTitle {
    color: #ffffff;
}

/* Tab Widget */
QTabWidget::pane {
    border: none;
    background-color: #1a1a1a;
}

QTabBar::tab {
    background-color: #252525;
    color: #888888;
    padding: 12px 24px;
    border: none;
    border-bottom: 2px solid transparent;
    font-weight: 500;
}

QTabBar::tab:selected {
    color: #ffffff;
    border-bottom: 2px solid #3b82f6;
}

QTabBar::tab:hover:!selected {
    color: #bbbbbb;
    background-color: #2a2a2a;
}

/* Lists */
QListWidget {
    background-color: #252525;
    border: 1px solid #333333;
    border-radius: 8px;
    padding: 8px;
    outline: none;
}

QListWidget::item {
    padding: 10px 12px;
    border-radius: 6px;
    margin: 2px 0;
}

QListWidget::item:selected {
    background-color: #3b82f6;
    color: #ffffff;
}

QListWidget::item:hover:!selected {
    background-color: #333333;
}

/* Table Widget */
QTableWidget {
    background-color: #252525;
    border: 1px solid #333333;
    border-radius: 8px;
    gridline-color: #333333;
    outline: none;
}

QTableWidget::item {
    padding: 8px;
    border: none;
}

QTableWidget::item:selected {
    background-color: #3b82f6;
    color: #ffffff;
}

QHeaderView::section {
    background-color: #2a2a2a;
    color: #e0e0e0;
    padding: 8px;
    border: none;
    border-bottom: 1px solid #333333;
    font-weight: 600;
}

/* Buttons */
QPushButton {
    background-color: #333333;
    color: #e0e0e0;
    border: none;
    border-radius: 6px;
    padding: 10px 20px;
    font-weight: 500;
}

QPushButton:hover {
    background-color: #404040;
}

QPushButton:pressed {
    background-color: #2a2a2a;
}

QPushButton#primaryButton {
    background-color: #3b82f6;
    color: #ffffff;
}

QPushButton#primaryButton:hover {
    background-color: #2563eb;
}

QPushButton#dangerButton {
    background-color: #dc2626;
    color: #ffffff;
}

QPushButton#dangerButton:hover {
    background-color: #b91c1c;
}

/* Line Edit */
QLineEdit {
    background-color: #252525;
    border: 1px solid #333333;
    border-radius: 6px;
    padding: 10px 12px;
    color: #e0e0e0;
}

QLineEdit:focus {
    border-color: #3b82f6;
}

QLineEdit::placeholder {
    color: #666666;
}

/* Text Edit */
QTextEdit, QPlainTextEdit {
    background-color: #252525;
    border: 1px solid #333333;
    border-radius: 8px;
    padding: 12px;
    color: #e0e0e0;
}

/* Scroll Bars */
QScrollBar:vertical {
    background-color: #1a1a1a;
    width: 10px;
    border-radius: 5px;
}

QScrollBar::handle:vertical {
    background-color: #404040;
    border-radius: 5px;
    min-height: 30px;
}

QScrollBar::handle:vertical:hover {
    background-color: #505050;
}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0;
}

QScrollBar:horizontal {
    background-color: #1a1a1a;
    height: 10px;
    border-radius: 5px;
}

QScrollBar::handle:horizontal {
    background-color: #404040;
    border-radius: 5px;
    min-width: 30px;
}

QScrollBar::handle:horizontal:hover {
    background-color: #505050;
}

QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0;
}

/* Cards / Group Boxes */
QFrame#card {
    background-color: #252525;
    border: 1px solid #333333;
    border-radius: 10px;
    padding: 16px;
}

/* Labels */
QLabel#sectionTitle {
    color: #ffffff;
    font-size: 15px;
    font-weight: 600;
}

QLabel#description {
    color: #888888;
    font-size: 12px;
}

/* Radio Buttons (for model selection) */
QRadioButton {
    spacing: 10px;
    color: #e0e0e0;
}

QRadioButton::indicator {
    width: 18px;
    height: 18px;
    border-radius: 9px;
    border: 2px solid #555555;
    background-color: #252525;
}

QRadioButton::indicator:checked {
    background-color: #3b82f6;
    border-color: #3b82f6;
}

QRadioButton::indicator:hover {
    border-color: #3b82f6;
}

/* Scroll Areas */
QScrollArea {
    border: none;
    background-color: transparent;
}
"""
