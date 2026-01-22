"""SQLite storage for transcription history."""

import logging
import sqlite3
from datetime import datetime
from pathlib import Path
from typing import List, Dict, Optional

logger = logging.getLogger(__name__)


class HistoryStorage:
    """SQLite-based transcription history storage."""

    def __init__(self, db_path: Optional[Path] = None):
        """
        Initialize history storage.

        Args:
            db_path: Path to SQLite database file
        """
        if db_path is None:
            # Store in app data directory
            db_path = Path(__file__).parent.parent.parent.parent / "data" / "history.db"

        self.db_path = db_path
        self._ensure_db()

    def _ensure_db(self):
        """Create database and tables if they don't exist."""
        self.db_path.parent.mkdir(parents=True, exist_ok=True)

        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()

        cursor.execute("""
            CREATE TABLE IF NOT EXISTS transcriptions (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT NOT NULL,
                text TEXT NOT NULL,
                model TEXT NOT NULL,
                duration_seconds REAL NOT NULL,
                created_at TEXT DEFAULT CURRENT_TIMESTAMP
            )
        """)

        # Index for faster timestamp queries
        cursor.execute("""
            CREATE INDEX IF NOT EXISTS idx_timestamp
            ON transcriptions(timestamp DESC)
        """)

        conn.commit()
        conn.close()
        logger.info(f"History database initialized: {self.db_path}")

    def add(self, text: str, model: str, duration: float) -> int:
        """
        Add a transcription to history.

        Args:
            text: Transcribed text
            model: Model used for transcription
            duration: Audio duration in seconds

        Returns:
            ID of inserted record
        """
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()

        timestamp = datetime.now().isoformat()

        cursor.execute("""
            INSERT INTO transcriptions (timestamp, text, model, duration_seconds)
            VALUES (?, ?, ?, ?)
        """, (timestamp, text, model, duration))

        record_id = cursor.lastrowid
        conn.commit()
        conn.close()

        logger.debug(f"Added history entry {record_id}: {text[:50]}...")
        return record_id

    def get_recent(self, limit: int = 100) -> List[Dict]:
        """
        Get recent transcriptions.

        Args:
            limit: Maximum number of entries to return

        Returns:
            List of transcription dictionaries
        """
        conn = sqlite3.connect(self.db_path)
        conn.row_factory = sqlite3.Row
        cursor = conn.cursor()

        cursor.execute("""
            SELECT id, timestamp, text, model, duration_seconds as duration
            FROM transcriptions
            ORDER BY timestamp DESC
            LIMIT ?
        """, (limit,))

        rows = cursor.fetchall()
        conn.close()

        return [dict(row) for row in rows]

    def clear_all(self):
        """Delete all history entries."""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        cursor.execute("DELETE FROM transcriptions")
        conn.commit()
        conn.close()
        logger.info("History cleared")

    def get_count(self) -> int:
        """Get total number of entries."""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        cursor.execute("SELECT COUNT(*) FROM transcriptions")
        count = cursor.fetchone()[0]
        conn.close()
        return count
