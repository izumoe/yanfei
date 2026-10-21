#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Capture engine and mode screens from the existing core WASM firmware.
Usage: python3 tools/capture_engine_guide.py EMULATOR_URL OUTPUT_DIRECTORY
Captures go to the requested directory; no firmware or stored device data changes.
"""
import base64
import json
import shutil
import sys
from pathlib import Path
from playwright.sync_api import sync_playwright

url=sys.argv[1];out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
with sync_playwright() as p:
    browser=p.chromium.launch(executable_path=shutil.which('chromium'),args=['--no-sandbox'])
    page=browser.new_page(ignore_https_errors=url.startswith('https:'))
    page.goto(url)
    captures=page.evaluate('''async()=>{
      const wasm=await(await fetch('felucca.wasm')).arrayBuffer(), images=[];
      async function device(engine=0,preset=0){
        const {instance}=await WebAssembly.instantiate(wasm,{}),x=instance.exports;
        x._initialize();x.web_nor_erase();x.web_boot();
        if(!x.web_core_profile())throw Error('Core firmware required');
        const render=n=>{for(let i=0;i<n;i+=128)x.web_render(Math.min(128,n-i));};
        const button=k=>{x.web_buttons(1<<k);render(2646);x.web_buttons(0);render(17640);};
        const key=m=>{x.web_keys(m);render(1764);};
        render(44100);x.web_test_core_sound(engine,preset);render(4410);button(2);button(6);
        x.web_midi(0x90,engine===10?36:60,90);render(4410);
        return {x,render,button,key};
      }
      function shot(d,name){
        const canvas=document.createElement('canvas');canvas.width=canvas.height=240;
        const ctx=canvas.getContext('2d'),pixels=ctx.createImageData(240,240),fb=new Uint16Array(d.x.memory.buffer,d.x.web_screen(),240*240);
        for(let i=0;i<fb.length;i++){const v=((fb[i]>>8)|(fb[i]<<8))&65535;pixels.data[i*4]=(v>>11&31)*255/31;pixels.data[i*4+1]=(v>>5&63)*255/63;pixels.data[i*4+2]=(v&31)*255/31;pixels.data[i*4+3]=255;}
        ctx.putImageData(pixels,0,0);
        const large=document.createElement('canvas');large.width=large.height=480;const c=large.getContext('2d');c.imageSmoothingEnabled=false;c.drawImage(canvas,0,0,480,480);
        images.push({name,png:large.toDataURL('image/png').split(',')[1]});
      }
      for(const [name,id] of [['analog',0],['nes',3],['dmg',4],['psg',5],['trio',6],['fm6',12],['ym',13],['wave',7],['phase',2],['string',8],['modal',9],['drum',10]]){
        const d=await device(id);shot(d,name+'-home');d.button(4);shot(d,name+'-edit1');d.button(4);shot(d,name+'-edit2');
      }
      for(const [name,id,preset] of [['dmg-wave',4,1],['psg-sega',5,5],['string-symp',8,1],['modal-memb',9,2]]){
        const d=await device(id,preset);shot(d,name+'-home');d.button(4);shot(d,name+'-edit1');d.button(4);shot(d,name+'-edit2');
        if(name==='psg-sega'){d.button(4);shot(d,'psg-sega-chip');}
      }
      for(const [name,button,count] of [['env',2,1],['env-dest',2,2],['lfo',3,1],['lfo-sync',3,2],['mod',3,4],['fx',0,1],['insert',0,3],['scale',1,1],['chord',1,2],['arp',8,1],['mixer',6,1],['clock',6,2],['step',9,2],['detail',9,3],['automation',9,4]]){
        const d=await device();for(let i=0;i<count;i++)d.button(button);shot(d,'mode-'+name);
      }
      return images;
    }''')
    for capture in captures:
        (out/(capture['name']+'.png')).write_bytes(base64.b64decode(capture['png']))
    manifest={'emulator':url,'capture_count':len(captures),'screens':[c['name']+'.png' for c in captures],'note':'Actual existing y13 WASM display buffers; nearest-neighbour 2x export.'}
    (out/'capture-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(manifest,indent=2));browser.close()
