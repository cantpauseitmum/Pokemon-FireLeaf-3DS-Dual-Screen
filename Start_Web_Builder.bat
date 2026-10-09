@echo off
title FireLeaf 3DS Web Builder
echo Starting local Web Builder server...
echo.

if not exist ".venv" (
    echo Creating isolated Python environment...
    python -m venv .venv
)

call .venv\Scripts\activate.bat
pip install flask --quiet

cd web_builder
python app.py
pause
