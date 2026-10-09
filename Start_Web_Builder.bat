@echo off
title FireLeaf 3DS Web Builder
echo Starting local Web Builder server...
echo.

python -c "import flask" >nul 2>&1
if %errorlevel% neq 0 (
    echo Installing Flask framework...
    python -m pip install flask
)

cd web_builder
python app.py
pause
