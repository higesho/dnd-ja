import re,sys
STR=re.compile(r'^\s*([A-Z0-9_]+)\s*=\s*"((?:[^"\\]|\\.)*)"',re.M)
def load(p):
    return dict(STR.findall(open(p,encoding='utf-8').read()))
if __name__=='__main__':
    en=load('orig/L_EN_US.LUA'); ja=load(sys.argv[1] if len(sys.argv)>1 else 'orig/L_JA_JP.LUA')
    print(len(en),len(ja))
    u=[k for k in en if k in ja and ja[k]==en[k] and re.search('[a-z]{2}',en[k])]
    print(len(u))
    for k in u: print(k,'=',en[k][:100])
    print('missing',[k for k in en if k not in ja])
