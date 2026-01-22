@echo off
echo ================================================
echo  Fixing PyTorch CUDA Installation
echo ================================================
echo.

echo Activating virtual environment...
call venv\Scripts\activate.bat

echo.
echo Uninstalling current PyTorch (no CUDA)...
pip uninstall torch torchvision torchaudio -y

echo.
echo Installing PyTorch with CUDA 12.1 support...
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu121

echo.
echo ================================================
echo  Verifying CUDA Installation
echo ================================================
python -c "import torch; print(f'\nCUDA Available: {torch.cuda.is_available()}'); print(f'CUDA Device: {torch.cuda.get_device_name(0) if torch.cuda.is_available() else \"None\"}'); print(f'CUDA Version: {torch.version.cuda if torch.cuda.is_available() else \"N/A\"}')"

echo.
echo ================================================
echo  If CUDA Available = True above, you're good!
echo  Now run: python -m src.medasr
echo ================================================
pause
