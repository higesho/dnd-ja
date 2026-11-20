# Apply the IWD2-style terminology to the UI strings: L_ja_JP.LUA -> L_ja_JP.restyled.LUA
import re
from luacmp import load,STR
from restyle import restyle
en=load('orig/L_EN_US.LUA')
src=open('L_ja_JP.LUA',encoding='utf-8').read()
n=0
def rep(m):
    global n
    k,v=m.group(1),m.group(2)
    t=restyle(en.get(k,''),v)
    if t!=v: n+=1
    return m.group(0).replace('"'+v+'"','"'+t+'"',1)
out=STR.sub(rep,src)
open('L_ja_JP.restyled.LUA','w',encoding='utf-8',newline='').write(out)
print('changed',n)
