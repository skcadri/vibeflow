@echo off
echo ================================================
echo  Installing PyTorch CPU Version (for Python 3.13)
echo  NOTE: This will be SLOW. For GPU support, use Python 3.12
echo ================================================
echo.

call venv\Scripts\activate.bat

echo Installing PyTorch CPU version...
pip install torch torchvision torchaudio

echo.
echo ================================================
echo  Verifying Installation
echo ================================================
python -c "import torch; print(f'\nPyTorch installed: {torch.__version__}'); print(f'Python version: {torch.version.python}')"

echo.
echo CPU version installed. Transcription will be SLOW.
echo For GPU acceleration, install Python 3.12 instead.
echo.
echo Now run: python -m src.medasr
pause
