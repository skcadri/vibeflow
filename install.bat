@echo off
echo Creating virtual environment...
python -m venv venv

echo Activating virtual environment...
call venv\Scripts\activate.bat

echo Installing CUDA PyTorch...
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu121

echo Installing other dependencies...
pip install -r requirements.txt

echo Done! To run the app:
echo   1. venv\Scripts\activate
echo   2. python -m src.medasr
pause
