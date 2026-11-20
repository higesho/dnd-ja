#!/bin/sh
# start the test game and go into the first Order of the Flame mission (champion select -> start)
cd "$(dirname "$0")/.."
W="python tools/win.py"
$W start "$@" && sleep 45
$W lclick 220 315; sleep 3; $W lclick 170 418; sleep 3; $W lclick 885 137; sleep 4; $W lclick 884 612; sleep 20
$W lclick 785 621; sleep 8
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18; do $W key esc; sleep 3; done
$W lclick 822 662; sleep 12
$W shot mission
