#!/bin/bash
echo "Uruchamianie lokalnego serwera Web Builder..."

# Sprawdzanie czy Flask jest zainstalowany
if ! python3 -c "import flask" &> /dev/null; then
    echo "Instalacja frameworka Flask..."
    python3 -m pip install flask
fi

# Uruchomienie aplikacji webowej
cd web_builder
python3 app.py
