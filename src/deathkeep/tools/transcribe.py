# ムービー（CINEMATZ/**/*.SMK）の音声を faster-whisper で書き起こす -> text/subs/en.json {name: [[start, end, text], ...]}
import os, json, subprocess, glob
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.abspath(os.path.join(ROOT, '..'))
from faster_whisper import WhisperModel

out_dir = os.path.join(ROOT, 'text', 'subs')
os.makedirs(out_dir, exist_ok=True)
wav_dir = os.path.join(ROOT, 'work', 'wav')
os.makedirs(wav_dir, exist_ok=True)
outp = os.path.join(out_dir, 'en.json')
res = json.load(open(outp, encoding='utf-8')) if os.path.exists(outp) else {}
model = WhisperModel('large-v3', device='cuda', compute_type='float16')
for f in sorted(glob.glob(os.path.join(GAME, 'CINEMATZ', '**', '*.SMK'), recursive=True)):
    name = os.path.basename(f)
    if name in res: continue
    wav = os.path.join(wav_dir, name + '.wav')
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', f, '-vn', '-ar', '16000', '-ac', '1', wav], check=False)
    if not os.path.exists(wav): res[name] = []; continue
    segs, info = model.transcribe(wav, language='en', vad_filter=True, beam_size=5)
    res[name] = [[round(s.start, 2), round(s.end, 2), s.text.strip()] for s in segs]
    print(name, len(res[name]), flush=True)
    json.dump(res, open(outp, 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
print('done')
