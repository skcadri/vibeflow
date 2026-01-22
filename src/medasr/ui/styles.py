"""Light theme QSS styles for MedASR settings window."""

LIGHT_THEME_QSS = """
/* Main Window */
QWidget {
    background-color: #ffffff;
    color: #1f2937;
    font-family: "Segoe UI", Arial, sans-serif;
    font-size: 13px;
}

/* Header */
QWidget#header {
    background-color: #f9fafb;
    border-bottom: 1px solid #e5e7eb;
}

QLabel#headerTitle {
    color: #111827;
}

/* Tab Widget */
QTabWidget::pane {
    border: none;
    background-color: #ffffff;
}

QTabBar::tab {
    background-color: #f9fafb;
    color: #6b7280;
    padding: 12px 24px;
    border: none;
    border-bottom: 2px solid transparent;
    font-weight: 500;
}

QTabBar::tab:selected {
    color: #111827;
    border-bottom: 2px solid #3b82f6;
}

QTabBar::tab:hover:!selected {
    color: #374151;
    background-color: #f3f4f6;
}

/* Lists */
QListWidget {
    background-color: #f9fafb;
    border: 1px solid #e5e7eb;
    border-radius: 8px;
    padding: 8px;
    outline: none;
}

QListWidget::item {
    padding: 10px 12px;
    border-radius: 6px;
    margin: 2px 0;
    color: #1f2937;
}

QListWidget::item:selected {
    background-color: #3b82f6;
    color: #ffffff;
}

QListWidget::item:hover:!selected {
    background-color: #e5e7eb;
}

/* Table Widget */
QTableWidget {
    background-color: #f9fafb;
    border: 1px solid #e5e7eb;
    border-radius: 8px;
    gridline-color: #e5e7eb;
    outline: none;
}

QTableWidget::item {
    padding: 8px;
    border: none;
    color: #1f2937;
}

QTableWidget::item:selected {
    background-color: #3b82f6;
    color: #ffffff;
}

QTableWidget::item:hover {
    background-color: #eff6ff;
}

QTableWidget::item:alternate {
    background-color: #f3f4f6;
}

QHeaderView::section {
    background-color: #f3f4f6;
    color: #374151;
    padding: 8px;
    border: none;
    border-bottom: 1px solid #e5e7eb;
    font-weight: 600;
}

/* Buttons */
QPushButton {
    background-color: #f3f4f6;
    color: #1f2937;
    border: 1px solid #d1d5db;
    border-radius: 6px;
    padding: 10px 20px;
    font-weight: 500;
}

QPushButton:hover {
    background-color: #e5e7eb;
    border-color: #9ca3af;
}

QPushButton:pressed {
    background-color: #d1d5db;
}

QPushButton#primaryButton {
    background-color: #3b82f6;
    color: #ffffff;
    border: 1px solid #3b82f6;
}

QPushButton#primaryButton:hover {
    background-color: #2563eb;
    border-color: #2563eb;
}

QPushButton#dangerButton {
    background-color: #dc2626;
    color: #ffffff;
    border: 1px solid #dc2626;
}

QPushButton#dangerButton:hover {
    background-color: #b91c1c;
    border-color: #b91c1c;
}

/* Line Edit */
QLineEdit {
    background-color: #ffffff;
    border: 1px solid #d1d5db;
    border-radius: 6px;
    padding: 10px 12px;
    color: #1f2937;
}

QLineEdit:focus {
    border-color: #3b82f6;
    outline: none;
}

QLineEdit::placeholder {
    color: #9ca3af;
}

/* Text Edit */
QTextEdit, QPlainTextEdit {
    background-color: #ffffff;
    border: 1px solid #d1d5db;
    border-radius: 8px;
    padding: 12px;
    color: #1f2937;
}

/* Scroll Bars */
QScrollBar:vertical {
    background-color: #f9fafb;
    width: 10px;
    border-radius: 5px;
}

QScrollBar::handle:vertical {
    background-color: #d1d5db;
    border-radius: 5px;
    min-height: 30px;
}

QScrollBar::handle:vertical:hover {
    background-color: #9ca3af;
}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0;
}

QScrollBar:horizontal {
    background-color: #f9fafb;
    height: 10px;
    border-radius: 5px;
}

QScrollBar::handle:horizontal {
    background-color: #d1d5db;
    border-radius: 5px;
    min-width: 30px;
}

QScrollBar::handle:horizontal:hover {
    background-color: #9ca3af;
}

QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0;
}

/* Cards / Group Boxes */
QFrame#card {
    background-color: #f9fafb;
    border: 1px solid #e5e7eb;
    border-radius: 10px;
    padding: 16px;
}

/* Labels */
QLabel#sectionTitle {
    color: #111827;
    font-size: 15px;
    font-weight: 600;
}

QLabel#description {
    color: #6b7280;
    font-size: 12px;
}

/* Radio Buttons (for model selection) */
QRadioButton {
    spacing: 10px;
    color: #1f2937;
}

QRadioButton::indicator {
    width: 18px;
    height: 18px;
    border-radius: 9px;
    border: 2px solid #d1d5db;
    background-color: #ffffff;
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

# Keep old name for compatibility
DARK_THEME_QSS = LIGHT_THEME_QSS
