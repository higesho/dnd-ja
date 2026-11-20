cd "/l/SteamLibrary/steamapps/common/Dungeons & Dragons Dark Sun Series/games/Dark Sun-ENG/_jp"
python -X utf8 tools/go_arena.py $1 > /dev/null 2>&1
until [ -n "$(python -X utf8 -c "
import sys; sys.path.insert(0,'tools'); import win; win.shot('_w')
from PIL import Image; im=Image.open('capture/_w.png'); print('ok' if im.getpixel((320,40))[1]>120 and im.getpixel((320,40))[0]<90 else '')" 2>/dev/null | grep ok)" ]; do sleep 5; done
sleep 3
python -X utf8 tools/talk_announcer.py 2
sleep 2
python -X utf8 tools/click_choice.py 317 170
echo done
