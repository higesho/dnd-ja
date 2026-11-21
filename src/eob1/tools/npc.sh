#!/bin/sh
# Test: run NPC sequence $1 from the level-1 start (rubble click) and capture as capture/$2.png
cd "$(dirname "$0")/.."
python tools/poke.py seq "$1" >/dev/null
python tools/win.py click 88 60
sleep 3
python tools/win.py shot "$2"
