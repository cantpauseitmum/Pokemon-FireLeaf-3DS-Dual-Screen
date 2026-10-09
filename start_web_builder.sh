#!/bin/bash
# Move to the directory where this script is located
cd "$(dirname "$0")"

echo "=================================================="
echo " Starting FireLeaf 3DS Web Builder... "
echo "=================================================="

if [ ! -d ".venv" ]; then
    echo "Creating isolated Python environment..."
    python3 -m venv .venv
fi
source .venv/bin/activate

pip install flask --quiet

cd web_builder
python app.py
