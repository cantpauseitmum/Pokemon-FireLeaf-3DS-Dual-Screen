#!/bin/bash
echo "Starting local Web Builder server..."

# Check if Flask is installed
if ! python3 -c "import flask" &> /dev/null; then
    echo "Installing Flask framework..."
    python3 -m pip install flask
fi

# Run the web application
cd web_builder
python3 app.py
