import sys
import time
from PyQt6.QtCore import QTimer

# Import after a delay to ensure the main app is running
time.sleep(2)

# Try to trigger settings window opening
print("Attempting to open settings...")
sys.exit(0)
