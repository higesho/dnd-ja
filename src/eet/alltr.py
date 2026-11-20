# load all translations: returns dict id->(m,f) for b (BGEE ids) and e (EET ids), plus en text
import json,glob
def load(prefix):
    t={}
    for f in sorted(glob.glob('out/%s[0-9][0-9][0-9]*.json'%prefix)):
        for k,v in json.load(open(f,encoding='utf-8')).items():
            t[int(k)]=(v,v) if isinstance(v,str) else (v['m'],v['f'])
    return t
def batches(prefix):
    en={}
    for f in glob.glob('batches/%s*.json'%prefix):
        for e in json.load(open(f,encoding='utf-8')): en[e['id']]=e
    return en
