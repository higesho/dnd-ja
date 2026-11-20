#!/bin/sh
# start -> new game "ali" -> Sinbar dialog done -> maze start (debug mode from test.conf)
W="python tools/win.py"
$W stop; $W start >/dev/null
sleep 14; $W key esc; sleep 4; $W key esc; sleep 4
$W key down; sleep 0.5; $W key enter; sleep 4
$W key text:ali enter; sleep 2
$W key down enter; sleep 10
python -c "
import sys; sys.path.insert(0,'tools'); import win
h,t=win.find_window()
for i in range(16): win.post_key(h,'up',hold=0.6)
"
sleep 1; $W key enter; sleep 2
for i in 1 2 3 4 5; do $W key enter; sleep 1.2; done
$W key down down enter; sleep 1.5
for i in 1 2 3 4 5 6 7; do $W key enter; sleep 1; done
sleep 2
