#!/bin/sh
# after go_play.sh: walk from Sinbar's clearing north to the old man in the corridor and talk
W="python tools/win.py"
$W key enter; sleep 1.5; $W key enter; sleep 3
python -c "
import sys,time; sys.path.insert(0,'tools'); import win
h,t=win.find_window()
for i in range(20): win.post_key(h,'up',hold=0.5)
for i in range(2): win.post_key(h,'left',hold=0.5)
win.post_key(h,'right',hold=0.5); win.post_key(h,'right',hold=0.3)
for i in range(12): win.post_key(h,'up',hold=0.5)
win.post_key(h,'down',hold=0.3); win.post_key(h,'left',hold=0.3)
"
$W key enter; sleep 1.5
