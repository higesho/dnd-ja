from tlk import read_tlk,write_tlk
from fixes import fix_all
for fn in ('dialog.tlk','dialogf.tlk'):
    lang,e=read_tlk('orig/ja_JP/'+fn)
    e=[list(x) for x in e]
    for x in e:
        s=x[4].decode('utf-8','replace'); t=fix_all(s)
        if t!=s: x[4]=t.encode('utf-8')
    def setj(i,s): e[i][4]=s.encode('utf-8'); e[i][0]|=1|4
    setj(15980,'<GAMEDAY>日目　<HOUR>時（<YEAR>年<DAYANDMONTH>）')
    setj(15981,'<MONTHNAME><DAY>日')
    setj(37823,'<MONTHNAME><DAY>日')
    write_tlk('../lang/ja_JP/'+fn,lang,[tuple(x) for x in e])
