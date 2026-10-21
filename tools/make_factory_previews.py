#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Audition new core factory patches using tools/render_factory.c and ffmpeg.
Run after building render_factory and writing its --list manifest into OUT.
Usage: python3 tools/make_factory_previews.py [build/core6] [--reuse-audio]
The preview assets are host artifacts; no audio is included in firmware.
"""
import concurrent.futures
import csv
import html
import json
from pathlib import Path
import re
import subprocess
import sys
import wave
import zipfile

root = Path(__file__).resolve().parents[1]
out = root / (sys.argv[1] if len(sys.argv) > 1 else 'build/core6')
manifest = json.loads((out/'factory-manifest.json').read_text())
notes = json.loads((root/'assets/presets/core-inspired-notes.json').read_text())
patches = [p for p in manifest['patches'] if p['preset'] >= 3]
assert len(patches) == 60 and len({p['name'] for p in patches}) == 60
for folder in ['audio','wav']:
    (out/folder).mkdir(exist_ok=True)

def slug(p):
    return re.sub(r'[^a-z0-9]+','-',p['engineName'].lower()+'-'+p['name'].lower()).strip('-')

def render(p):
    name = slug(p)
    wav = out/'wav'/(name+'.wav')
    cp = subprocess.run([str(root/'build/host/render_factory'),str(p['engine']),str(p['preset']),str(wav)],capture_output=True,text=True,check=True)
    stats = json.loads(cp.stdout)
    assert stats['rail_samples'] == 0 and 0 < stats['peak'] < 1 and stats['rms'] > 0
    subprocess.run(['ffmpeg','-y','-loglevel','error','-i',str(wav),'-codec:a','libmp3lame','-q:a','3',str(out/'audio'/(name+'.mp3'))],check=True)
    return {**stats,'preset':p['preset'],'file':'audio/'+name+'.mp3'}

if '--reuse-audio' in sys.argv[2:]:
    stats = json.loads((out/'audition-health.json').read_text())
    assert [s['name'] for s in stats] == [p['name'] for p in patches]
    assert all((out/s['file']).exists() for s in stats)
else:
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        stats = list(pool.map(render,patches))
(out/'audition-health.json').write_text(json.dumps(stats,indent=2)+'\n')
with (out/'preset-catalog.csv').open('w',newline='') as f:
    w=csv.writer(f);w.writerow(['engine','number_in_engine','name','category','inspiration','preview'])
    for p,s in zip(patches,stats):
        w.writerow([p['engineName'],p['preset']+1,p['name'],p['category'],notes[p['engineName']][p['name']],s['file']])
# Optional desktop-library backup; all 96 are already factory defaults in core6.
seen = {};keys=[]
for label in manifest['labels']:
    seen[label]=seen.get(label,0)+1
    keys.append(label if seen[label]==1 else f"{label}#{seen[label]}")
keys += [f'E{k}' for k in range(8)]
library={'format':'felucca-library','version':1,'kind':'library','firmware':'v1.6.1-core6',
         'pCount':manifest['pCount'],'pE0':manifest['pE0'],'paramLabels':keys,'engines':manifest['engines'],
         'patches':[{**{k:p[k] for k in ['name','engine','engineName','params']},
                     'pattern':None,'tags':[p['engineName'].lower(),p['category'].lower()]+(['inspired'] if p['preset']>=3 else ['original']),
                     **({'fm6':p['fm6']} if 'fm6' in p else {})} for p in manifest['patches']]}
(out/'factory-library.json').write_text(json.dumps(library,indent=2)+'\n')
# A 48-second sampler: the first addition from each engine, at its own level.
with wave.open(str(out/'wav/highlights.wav'),'wb') as w:
    w.setparams((2,2,44100,0,'NONE','not compressed'))
    for p in patches:
        if p['preset']==3:
            with wave.open(str(out/'wav'/(slug(p)+'.wav')),'rb') as src:
                w.writeframes(src.readframes(src.getnframes()))
subprocess.run(['ffmpeg','-y','-loglevel','error','-i',str(out/'wav/highlights.wav'),'-codec:a','libmp3lame','-q:a','3',str(out/'highlights.mp3')],check=True)
sections=[]
for engine in dict.fromkeys(p['engineName'] for p in patches):
    cards=[]
    for p in patches:
        if p['engineName']!=engine:continue
        n=html.escape(p['name']);desc=html.escape(notes[engine][p['name']])
        cards.append(f'<article><h3>{p["preset"]+1}. {n}</h3><p>{desc}</p><audio controls preload="none" src="audio/{slug(p)}.mp3"></audio></article>')
    sections.append(f'<section><h2>{engine}</h2><div class="grid">'+''.join(cards)+'</div></section>')
page='''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>Felucca core6 — inspired presets</title>
<style>body{background:#17191c;color:#e8e8e8;font:16px/1.5 system-ui,sans-serif;margin:32px auto;padding:0 20px;max-width:1120px}h1,h2,h3{line-height:1.2}h2{margin-top:40px}h3{font-size:16px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(290px,1fr));gap:14px}article{border:1px solid #50545c;border-radius:8px;padding:16px}article p{min-height:48px;color:#c7c9cf;font-size:14px}audio{max-width:100%;width:100%}a{color:#b9caff}</style>
<h1>Inspired presets</h1><p>60 additions to core6's default library: eight presets per engine, 96 in total. New sounds occupy positions 4–8 in each engine; the original three stay in place.</p>
<p>Original patch recipes inspired by classic synth and game-hardware timbres. The clips use a short original phrase and the instrument's own effects and level. These are interpretations, not factory ROM patches, samples or exact instrument recreations.</p>
<h2>48-second sampler</h2><audio controls preload="none" src="highlights.mp3"></audio>'''+''.join(sections)+'''
<script>document.addEventListener('play',e=>{if(e.target.tagName==='AUDIO')document.querySelectorAll('audio').forEach(a=>{if(a!==e.target)a.pause()})},true)</script></html>'''
(out/'preset-catalog.html').write_text(page)
with zipfile.ZipFile(out/'preset-auditions.zip','w',zipfile.ZIP_DEFLATED) as z:
    for f in [out/'preset-catalog.html',out/'preset-catalog.csv',out/'highlights.mp3',*sorted((out/'audio').glob('*.mp3'))]:
        z.write(f,f.relative_to(out))
print(f"auditions: {len(stats)} clips, zero rails, peak {max(s['peak'] for s in stats):.6f} FS; 48-second sampler and offline catalog written")
