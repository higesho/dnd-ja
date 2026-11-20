# 語りのあるムービーを単語の時刻つきで書き起こし直し、文ごとの字幕に分ける -> text/subs/en_cues.json
#   {name: {"frames": n, "w": w, "h": h, "cues": [[start, end, "text"], ...]}}
import os, json, struct, re, glob
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.abspath(os.path.join(ROOT, '..'))
from faster_whisper import WhisperModel

NARRATED = ['EGB', 'R1EA', 'R1IB', 'R2EB', 'R2IC', 'R3IA', 'R3IC', 'R3IDD', 'R3IDE', 'R3IDH', 'TEASER']
PROMPT = ('DeathKeep. The Necromancer. A dwarven stronghold in the frozen wastes. '
          'Your ancestors, the amulet, the altar stone, the gem. The spire. Ha ha ha!')
model = WhisperModel('large-v3', device='cuda', compute_type='float16')
out = {}
for f in sorted(glob.glob(os.path.join(GAME, 'CINEMATZ', '**', '*.SMK'), recursive=True)):
    name = os.path.basename(f)[:-4]
    if name not in NARRATED: continue
    hdr = open(f, 'rb').read(16)
    w, h, frames = struct.unpack_from('<III', hdr, 4)
    wav = os.path.join(ROOT, 'work', 'wav', name + '.SMK.wav')
    segs, info = model.transcribe(wav, language='en', beam_size=5, word_timestamps=True,
                                  initial_prompt=PROMPT, vad_filter=False, condition_on_previous_text=True)
    words = [wd for s in segs for wd in (s.words or [])]
    cues, cur = [], []
    for wd in words:
        cur.append(wd)
        txt = ''.join(x.word for x in cur).strip()
        end_sent = re.search(r'[.!?]["\']?$', wd.word.strip())
        if (end_sent and len(txt) > 25) or len(txt) > 90 or (end_sent and cur[-1].end - cur[0].start > 3):
            cues.append([round(cur[0].start, 2), round(cur[-1].end, 2), txt]); cur = []
    if cur: cues.append([round(cur[0].start, 2), round(cur[-1].end, 2), ''.join(x.word for x in cur).strip()])
    out[name] = {'w': w, 'h': h, 'frames': frames, 'cues': cues}
    print(name, w, h, frames, len(cues), flush=True)
    for c in cues: print('   %6.2f %6.2f %s' % tuple(c), flush=True)
json.dump(out, open(os.path.join(ROOT, 'text', 'subs', 'en_cues.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
print('done')
