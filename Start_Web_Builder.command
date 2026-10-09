#!/bin/bash
# Move to the directory where this script is located
cd "$(dirname "$0")"

echo "=================================================="
echo " Starting FireLeaf 3DS Web Builder... "
echo "=================================================="

# Create and activate virtual environment
if [ ! -d ".venv" ]; then
    echo "Creating isolated Python environment..."
    python3 -m venv .venv
fi
source .venv/bin/activate

# Install Flask quietly
pip install flask --quiet

# Run the web application
cd web_builder
python app.py
