# SPDX-License-Identifier: GPL-3.0-only
# Serve build/emu first, then: python3 tools/capture_tracker.py [URL] [OUTDIR]
from playwright.sync_api import sync_playwright
from pathlib import Path
import json
import shutil
import sys

root = Path(__file__).resolve().parents[1]
url = sys.argv[1] if len(sys.argv) > 1 else 'http://127.0.0.1:8790/'
out = Path(sys.argv[2]) if len(sys.argv) > 2 else root / 'build/yanfei13'
out.mkdir(parents=True, exist_ok=True)
with sync_playwright() as p:
    b = p.chromium.launch(executable_path=shutil.which('chromium'), headless=True, args=['--no-sandbox'])
    page = b.new_page(viewport={'width':1056,'height':650}, device_scale_factor=1)
    page.goto(url)
    result = page.evaluate('''async()=>{
      const bytes=await (await fetch('felucca.wasm')).arrayBuffer();
      document.body.innerHTML='<main style="display:grid;grid-template-columns:repeat(4,240px);gap:16px;padding:24px;background:#151515;color:white;font:13px monospace"></main>';
      document.body.style.margin='0';const main=document.querySelector('main');let checks=[];
      const assert=(ok,s)=>{if(!ok)throw Error(s);checks.push(s);};
      async function device(engine=0,preset=2,detail=true){
        const {instance}=await WebAssembly.instantiate(bytes,{});const x=instance.exports;
        x._initialize();x.web_nor_erase();x.web_boot();
        const render=n=>{for(let i=0;i<n;i+=128)x.web_render(Math.min(128,n-i));};
        const key=m=>{x.web_keys(m);render(1764);};
        const tap=k=>{key(1<<k);key(0);};
        const button=k=>{x.web_buttons(1<<k);render(2646);x.web_buttons(0);render(17640);};
        const enc=(k,n)=>{x.web_enc(k,n);render(1764);};
        const row=()=>x.web_test_tracker_state()&63;
        const move=r=>enc(0,r-row());
        const steps=()=>new Uint8Array(x.memory.buffer,x.web_test_steps(0),64*11);
        render(44100);x.web_test_core_sound(engine,preset);render(4410);button(9);
        assert((x.web_test_tracker_library_state()&3)===3,'fresh boot opens ALL by default');
        if(detail){key(1<<21);key((1<<21)|(1<<3));key(1<<21);key(0);}
        return {x,render,key,tap,button,enc,row,move,steps};
      }
      function shot(d,name){
        const fig=document.createElement('figure');fig.style.margin='0';
        const cap=document.createElement('figcaption');cap.textContent=name;cap.style.marginBottom='8px';fig.append(cap);
        const canvas=document.createElement('canvas');canvas.width=canvas.height=240;fig.append(canvas);main.append(fig);
        const ctx=canvas.getContext('2d'),im=ctx.createImageData(240,240);
        const fb=new Uint16Array(d.x.memory.buffer,d.x.web_screen(),240*240);
        for(let i=0;i<fb.length;i++){const v=((fb[i]>>8)|(fb[i]<<8))&65535;im.data[i*4]=((v>>11)&31)*255/31;im.data[i*4+1]=((v>>5)&63)*255/63;im.data[i*4+2]=(v&31)*255/31;im.data[i*4+3]=255;}
        ctx.putImageData(im,0,0);
      }
      const d=await device(0,2,false);shot(d,'SEQ: default ALL view');
      d.key(1<<21);d.key((1<<21)|(1<<3));d.key(1<<21);d.key(0);
      assert((d.x.web_test_tracker_library_state()&3)===0,'SHIFT+UP opens detailed PHRASE');
      d.key(1<<21);d.key((1<<21)|(1<<4));d.key(1<<21);d.key(0);d.button(9);
      assert((d.x.web_test_tracker_library_state()&3)===0,'returning to tracker keeps the chosen view');
      assert((d.x.web_test_tracker_state()>>12&15)===4,'tracker navigation mode');
      await d.button(11);
      assert((d.x.web_test_tracker_state()>>12&15)===5,'REC enables note mode');
      for(const [r,ks] of [[0,[7,11,14,19]],[4,[5,9,12,17]],[8,[3+5,12,15,20]],[12,[5,10,14,17]]]){
        d.move(r);d.key(ks.reduce((m,k)=>m|1<<k,0));d.key(0);
        assert(d.steps()[r*11+4]===4,'four notes retained on row '+r);
      }
      d.move(0);d.render(88200);shot(d,'Four-note chord rows');
      const memoryBefore=d.steps().slice();
      const bankBefore=new Uint8Array(d.x.memory.buffer,d.x.web_test_tracker_bank(),d.x.web_test_tracker_bank_size()).slice();
      // Save a project through the real panel, then reboot with its NOR image.
      for(let i=0;i<4;i++)d.button(7);
      d.enc(6,1);d.button(13);d.button(13);d.render(4410);
      assert(d.x.web_flash_writes_count()>0,'project writes NOR through panel');
      const saved=new Uint8Array(d.x.memory.buffer,d.x.web_nor(),d.x.web_nor_size()).slice();
      const {instance}=await WebAssembly.instantiate(bytes,{});const y=instance.exports;
      y._initialize();y.web_nor_erase();new Uint8Array(y.memory.buffer,y.web_nor(),y.web_nor_size()).set(saved);y.web_boot();
      const advance=n=>{for(let i=0;i<n;i+=128)y.web_render(Math.min(128,n-i));};advance(44100);
      const by=k=>{y.web_buttons(1<<k);advance(2646);y.web_buttons(0);advance(17640);};
      for(let i=0;i<4;i++)by(7);
      y.web_enc(5,1);advance(1764);by(13);advance(4410);
      const restored=new Uint8Array(y.memory.buffer,y.web_test_steps(0),64*11);
      assert(memoryBefore.every((v,i)=>v===restored[i]),'project survives fresh WASM instance / NOR reload');
      const bankAfter=new Uint8Array(y.memory.buffer,y.web_test_tracker_bank(),y.web_test_tracker_bank_size());
      assert(bankBefore.every((v,i)=>v===bankAfter[i]),'complete phrase/instrument library survives NOR reload');
      const play=await device();play.button(11);play.key((1<<7)|(1<<11)|(1<<14));play.key(0);
      play.move(0);play.button(6);play.button(10);play.render(13230);shot(play,'Playback with follow');
      assert(play.x.web_playing()===1,'tracker playback starts');
      let peak=0;for(let j=0;j<128;j++){play.x.web_render(128);for(const v of new Float32Array(play.x.memory.buffer,play.x.web_out_l(),128))peak=Math.max(peak,Math.abs(v));}
      assert(peak>0&&peak<0.99,'tracker notes produce bounded PCM');
      const drum=await device(10,3);
      for(const [r,cs] of [[0,[0,2]],[2,[2]],[4,[1,2]],[6,[2]],[8,[0,2]],[10,[2,3]],[12,[1,6]],[14,[2,7]]]){
        drum.move(r);let col=(drum.x.web_test_tracker_state()>>6)&15;
        for(const c of cs){while(col<c){drum.tap(4);col++;}while(col>c){drum.tap(0);col--;}drum.tap(26);}
      }
      drum.move(4);drum.render(88200);shot(drum,'Eight drum lanes');
      assert(drum.steps()[8]===5&&drum.steps()[4*11+8]===6,'simultaneous drum hits retained');
      const a=await device();a.tap(26);
      const combo=(mask,k)=>{a.key(mask);a.key(mask|1<<k);a.key(mask);a.key(0);};
      a.key(1<<24);a.enc(0,64);a.key(0);shot(a,'Per-row gate: 50%');
      combo(1<<21,0);assert((a.x.web_test_tracker_library_state()&3)===1,'SHIFT+LEFT opens chain');
      shot(a,'Chain: phrases + transpose');
      combo((1<<21)|(1<<24),26);
      combo(1<<21,4);assert((a.x.web_test_tracker_library_state()>>>17)>=16,'clone opens independent phrase');
      combo(1<<21,0);combo(1<<21,0);
      assert((a.x.web_test_tracker_library_state()&3)===2,'SHIFT+LEFT opens song');
      combo(1<<21,24);a.tap(2);combo(1<<21,26);
      assert([0,1,2,3].every(k=>a.x.web_test_tracker_order(1,k)===a.x.web_test_tracker_order(0,k)),'copy/paste complete four-track section');
      combo((1<<21)|(1<<24),26);shot(a,'Song: copied section + variation');
      a.tap(23);assert(a.x.web_playing()===1,'song arrangement starts');shot(a,'Song arrangement playing');
      a.tap(23);a.render(4410);
      const arranged=new Uint8Array(a.x.memory.buffer,a.x.web_test_tracker_bank(),a.x.web_test_tracker_bank_size()).slice();
      for(let i=0;i<4;i++)a.button(7);a.enc(6,1);a.button(13);a.button(13);a.render(4410);
      const norCopy=new Uint8Array(a.x.memory.buffer,a.x.web_nor(),a.x.web_nor_size()).slice();
      const fresh=(await WebAssembly.instantiate(bytes,{})).instance.exports;
      fresh._initialize();fresh.web_nor_erase();new Uint8Array(fresh.memory.buffer,fresh.web_nor(),fresh.web_nor_size()).set(norCopy);fresh.web_boot();
      const rf=n=>{for(let i=0;i<n;i+=128)fresh.web_render(Math.min(128,n-i));};rf(44100);
      const bf=k=>{fresh.web_buttons(1<<k);rf(2646);fresh.web_buttons(0);rf(17640);};
      for(let i=0;i<4;i++)bf(7);fresh.web_enc(5,1);rf(1764);bf(13);rf(4410);
      const reloaded=new Uint8Array(fresh.memory.buffer,fresh.web_test_tracker_bank(),fresh.web_test_tracker_bank_size());
      assert(arranged.every((v,i)=>v===reloaded[i]),'cloned phrases, gates, chains and copied song sections survive fresh boot');
      const o=await device(0,2,false);
      const oc=(mask,k)=>{o.key(mask);o.key(mask|1<<k);o.key(mask);o.key(0);};
      assert((o.x.web_test_tracker_library_state()&3)===3,'ALL is editable from first entry');
      o.button(11);
      const patterns=[[[0,[7]],[2,[7,11]],[4,[7,11,14]],[6,[7,8,9]],[8,[7,11,14,19]]],
                      [[0,[5]],[4,[9,12]],[8,[5,9,12]],[12,[5,10,14,17]]],
                      [[0,[7]],[3,[9]],[6,[11]],[9,[12]],[12,[14]]]];
      for(let k=0;k<3;k++){
        for(const [r,ks] of patterns[k]){o.move(r);o.key(ks.reduce((m,n)=>m|1<<n,0));o.key(0);}
        if(k<2)o.enc(1,1);
      }
      o.enc(1,1);
      for(const r of [0,4,8,12]){o.move(r);o.tap(26);}
      for(const [r,ks] of [[2,[8]],[6,[5,8]],[10,[5,8,10]],[14,[8]]]){
        o.move(r);o.key(ks.reduce((m,k)=>m|1<<k,0));o.key(0);
      }
      const hits=new Uint8Array(o.x.memory.buffer,o.x.web_test_steps(3),64*11);
      assert(hits[8]===1&&hits[10*11+8]!==0,'ALL drum cells support direct note entry');
      o.enc(1,-1);
      o.move(4);o.tap(0);assert((o.x.web_test_tracker_state()>>>24)===1&&o.row()===4,'ALL track navigation keeps selected row');
      const p1=()=>new Uint8Array(o.x.memory.buffer,o.x.web_test_steps(1),64*11);
      const before=p1()[4*11];oc(1<<26,4);assert(p1()[4*11]===before+1,'ALL edit transposes stored note');
      oc(1<<21,24);o.move(10);oc(1<<21,26);assert(p1()[10*11]===p1()[4*11],'ALL copy/paste edits selected row');
      o.move(4);o.render(88200);shot(o,'ALL: editable four-track overview');
      oc(1<<21,3);assert((o.x.web_test_tracker_library_state()&3)===0,'ALL switches back to detailed notes');
      oc(1<<21,3);o.button(10);o.render(8820);assert(o.x.web_playing()===1,'ALL starts four-track playback');
      shot(o,'ALL: independent track playheads');o.button(10);
      const boot=(await WebAssembly.instantiate(bytes,{})).instance.exports;
      boot._initialize();boot.web_nor_erase();boot.web_boot();shot({x:boot},'Yanfei startup');
      const c=await device(0,2,false);
      const state=()=>c.x.web_test_tracker_section_state();
      const pg=()=>state()&3;
      const abs=()=>pg()*16+c.row();
      const go=r=>c.enc(0,r-abs());
      const cc=(mask,k)=>{c.key(mask);c.key(mask|1<<k);c.key(mask);c.key(0);};
      assert((state()>>3&31)===16 && (state()&4)!==0,'new section is four bars');
      shot(c,'Section: page 1 of 4');
      go(63);assert(pg()===3 && c.row()===15,'SELECT scrolls to row 3F on page 4');
      c.button(11);c.key((1<<7)|(1<<11));c.key(0);
      assert(c.steps()[15*11+4]===2 && abs()===63,'last section row accepts notes and clamps cursor');
      c.render(88200);shot(c,'Section: editable row 3F');
      cc((1<<21)|(1<<24),0);go(0);cc((1<<21)|(1<<24),4);
      const shared=c.x.web_test_tracker_library_state()>>>17;
      cc((1<<21)|(1<<24),26);
      assert((c.x.web_test_tracker_library_state()>>>17)!==shared,'page clone creates separate phrase through keys');
      c.key(1<<21);c.enc(0,1);c.key(0);
      assert(pg()===1,'SHIFT + SELECT changes section page');
      c.key((1<<21)|(1<<24));c.enc(0,-2);c.key(0);
      assert((state()>>3&31)===8,'section length control sets two bars');
      c.key(1<<24);c.enc(2,-1);c.key(0);
      assert((state()>>8&1)===0,'OPTION + PRESETS sets selected track to WAIT');
      c.button(5); // GLO opens SONG.
      assert((c.x.web_test_tracker_library_state()&3)===2,'section opens SONG');
      shot(c,'Song: two bars, repeat/wait flags');
      cc(1<<21,3);assert((c.x.web_test_tracker_library_state()&3)===3 && pg()===0,'SONG SHIFT + UP returns to section');
      c.button(10);c.render(100000);
      assert(c.x.web_playing()===1 && (state()>>20&15)===5,'section PLAY uses looping timed transport');
      shot(c,'Section playback across pages');c.button(10);
      // Additional operating screens for the illustrated guide.
      const h=await device(0,3,false);h.button(2);h.button(6);h.render(88200);shot(h,'HOME: select and edit a sound');
      const detail=await device();detail.button(11);detail.key((1<<7)|(1<<11)|(1<<14));detail.key(0);detail.move(0);
      detail.button(9);detail.button(9);detail.render(88200);shot(detail,'DETAIL: chance repeats nudge velocity');
      detail.button(9);detail.render(88200);shot(detail,'AUTOMATION: locks and events');
      const save=await device(0,2,false);save.tap(26);
      for(let i=0;i<4;i++)save.button(7);save.render(88200);shot(save,'PROJECT: save the whole song');
      save.enc(6,1);save.button(13);save.render(88200);shot(save,'NAME: confirm the project write');
      const aa=await device(0,2,false);aa.tap(26);aa.button(5);
      const ac=(mask,k)=>{aa.key(mask);aa.key(mask|1<<k);aa.key(mask);aa.key(0);};
      ac(1<<21,24);
      for(let i=0;i<3;i++){aa.tap(2);ac(1<<21,26);}
      aa.tap(3);ac((1<<21)|(1<<24),26);aa.render(88200);shot(aa,'SONG: A A B A chain variation');
      assert(aa.x.web_test_tracker_order(0,0)===aa.x.web_test_tracker_order(1,0) &&
             aa.x.web_test_tracker_order(0,0)===aa.x.web_test_tracker_order(3,0) &&
             aa.x.web_test_tracker_order(2,0)!==aa.x.web_test_tracker_order(0,0),'illustrated A A B A uses a separate chain for B');
      ac(1<<21,4);ac(1<<21,24);aa.tap(2);ac(1<<21,26);aa.tap(4);aa.enc(2,12);
      aa.render(88200);shot(aa,'CHAIN: a shared phrase with transpose');
      const chip=await device(5,3,false);chip.button(2);
      for(let i=0;i<3;i++)chip.button(4);chip.render(88200);shot(chip,'PSG: AY chip controls');
      chip.enc(3,1);chip.render(88200);shot(chip,'PSG: Sega chip controls');
      const wave=await device(7,3,false);wave.button(2);wave.button(4);wave.render(88200);shot(wave,'WAVE: select a wave bank');
      return {checks,peak};
    }''')
    page.locator('main').screenshot(path=str(out / 'tracker-screens.png'))
    page.locator('canvas').nth(1).screenshot(path=str(out / 'selected-note-footer.png'))
    page.locator('canvas').nth(8).screenshot(path=str(out / 'tracker-overview.png'))
    page.locator('canvas').nth(10).screenshot(path=str(out / 'yanfei-startup.png'))
    page.locator('canvas').nth(12).screenshot(path=str(out / 'section-page4.png'))
    page.locator('canvas').nth(13).screenshot(path=str(out / 'section-song.png'))
    # Save a crisp 2x view of each actual 240 x 240 screen for guide figures.
    screens = out / 'screens'; screens.mkdir(exist_ok=True)
    page.locator('main').evaluate("e=>e.style.display='block'")
    for index, figure in enumerate(page.locator('figure').all()):
        canvas = figure.locator('canvas')
        canvas.evaluate("e=>{e.style.width='480px';e.style.height='480px';e.style.imageRendering='pixelated'}")
        canvas.screenshot(path=str(screens / f'{index:02d}.png'))
    (out / 'browser-checks.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result))
    b.close()
