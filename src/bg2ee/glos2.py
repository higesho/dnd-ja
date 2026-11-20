import pickle,collections,re
exec(open('glos.py',encoding='utf-8').read().split('for t in sys.argv')[0])
dic=pickle.load(open('dic.pkl','rb'))
ut=[E[i] if i<len(E) else '' for i in sorted(unt)]
from tlk import read_tlk
_,en2=read_tlk(G+'lang/en_US/dialog.tlk')
text=' '.join(en2[i][4].decode('utf-8','replace') for i in sorted(unt))
cand=collections.Counter(re.findall(r"\b([A-Z][a-z']+(?:[ -](?:of |the |de |Il-)?[A-Z][a-z']+)*)",text))
cand.update(re.findall(r"\b((?:githyanki|mind flayers?|tanar'ri|baatezu|umber hulks?|djinni?|efreeti|beholders?|illithids?|yuan-ti|duergar|drow|kuo-toa|rakshasas?|liches?|vampires?|minotaurs?|gnolls?|ogres?|trolls?|golems?|imps?|wyverns?|salamanders?|myconids?|hobgoblins?|bugbears?|kobolds?|gnomes?|halflings?|dwarves|dwarf|elves|elf|orcs?|half-orcs?|tieflings?|aasimars?|genasi|slaads?|devils?|demons?)\b)",text,re.I))
stop=set(open('stop.txt').read().split()) if False else set()
res=[]
for k,v in cand.most_common():
    if v<2: break
    if k.endswith("'s"): continue
    d=dic.get(k)
    if d: res.append((k,v,'DIC',d.most_common(2))); continue
    n,l=look(k)
    res.append((k,v,n,l))
pickle.dump(res,open('glos_auto.pkl','wb'))
for r in res: print(r)
