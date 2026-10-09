#!/bin/bash
# Move to the directory where this script is located
cd "$(dirname "$0")"

echo "=================================================="
echo " Starting FireLeaf 3DS Web Builder... "
echo "=================================================="

# Check if Flask is installed
if ! python3 -c "import flask" &> /dev/null; then
    echo "Installing Flask framework..."
    python3 -m pip install flask
fi

# Run the web application
cd web_builder
python3 app.py
