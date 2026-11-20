#!/bin/sh
# new game -> walk up to Sinbar -> talk (first dialog). Assumes the game was started with win.py start
W="python tools/win.py"
sleep 14; $W key esc; sleep 4; $W key esc; sleep 4
$W shot menu >/dev/null
$W key down enter; sleep 4
$W key text:ali enter; sleep 2
$W key down enter; sleep 10
python -c "
import sys; sys.path.insert(0,'tools'); import win
h,t=win.find_window()
for i in range(16): win.post_key(h,'up',hold=0.6)
"
sleep 1; $W key enter; sleep 2; $W shot talk1
