# walk DIR SECONDS [DIR SECONDS ...] then shot NAME:  python tools/walk.py up 2 right 1 shot a1
import sys, time
sys.path.insert(0, 'tools'); import win
h, t = win.find_window()
a = sys.argv[1:]
i = 0
while i < len(a):
    if a[i] == 'shot': win.shot(a[i + 1]); i += 2; continue
    if a[i] == 'key': win.post_key(h, a[i + 1]); time.sleep(0.5); i += 2; continue
    d, s = a[i], float(a[i + 1]); i += 2
    n = max(1, int(s / 0.5))
    for k in range(n): win.post_key(h, d, hold=0.5)
