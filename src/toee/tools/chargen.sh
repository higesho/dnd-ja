#!/bin/sh
# drive the game from the main menu to the feats page of character creation (test harness)
W="python tools/win.py"
$W click 640 480; sleep 3; $W click 640 480; sleep 4          # new game, normal
$W click 508 249; sleep 1; $W click 560 526; sleep 4          # LG party, accept
$W click 352 170; sleep 4                                     # create
$W click 800 290; sleep 1
for i in 0 1 2 3 4 5; do y=$((219+31*i)); $W drag 687 $y 571 $y; done; sleep 1
$W click 966 184; sleep 2; $W click 681 202; sleep 1           # race human
$W click 966 214; sleep 2; $W click 605 322; sleep 1           # male
$W click 966 244; sleep 2; $W click 966 274; sleep 2; $W click 570 230; sleep 1   # height, hair
$W click 966 304; sleep 2; $W click 612 214; sleep 1           # class barbarian
$W click 966 334; sleep 2; $W click 678 231; sleep 1           # NG
$W click 966 364; sleep 2; $W click 765 193; sleep 1           # Kord
$W click 966 424; sleep 2                                      # feats
