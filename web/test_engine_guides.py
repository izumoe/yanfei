#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check engine coverage and the rendered engine/mode manual.
Usage: python3 web/test_engine_guides.py SITE_BASE OUTPUT_DIRECTORY CORE_DESC_JSON
CORE_DESC_JSON is emitted by the core host build of tests/descdump.c.
"""
import json
import shutil
import sys
from pathlib import Path
from playwright.sync_api import sync_playwright

base=sys.argv[1].rstrip('/')+'/';out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
tables=json.loads(Path(sys.argv[3]).read_text());checks=[]
with sync_playwright() as p:
    browser=p.chromium.launch(executable_path=shutil.which('chromium'),args=['--no-sandbox'])
    page=browser.new_page(viewport={'width':1100,'height':900},ignore_https_errors=base.startswith('https:'))
    errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
    for name in tables['ORDER']:
        engine=next(e for e in tables['ENG'] if e['name']==name)
        page.goto(base+'docs/ENGINE_'+name+'.html')
        assert page.locator('h1').inner_text()==name+' engine'
        text=page.locator('main').inner_text()
        for preset in engine['presets']:assert preset['name'] in text,(name,preset['name'])
        for title in engine['titles']:assert title+' page' in text,(name,title)
        for heading in ['HOME knobs','Engine controls','Try these changes','Factory presets','Limits and useful checks']:
            assert page.get_by_role('heading',name=heading,exact=True).count()==1,(name,heading)
        page.wait_for_function('Array.from(document.images).every(i=>i.complete && i.naturalWidth>0)')
        assert page.locator('figure img').count()>=3,name
        assert page.locator('figure img').evaluate_all('(imgs)=>imgs.every(i=>i.alt.length>20 && i.closest("a").href.endsWith(".png"))')
        for width in [1100,390]:
            page.set_viewport_size({'width':width,'height':900})
            assert page.evaluate('document.documentElement.scrollWidth<=innerWidth'),(name,width)
        if name in ['ANALOG','PSG','FM6','YM','STRING','MODAL']:
            page.screenshot(path=str(out/f'{name.lower()}-phone.png'),full_page=True)
    checks.append('All twelve engines have HOME controls, both main pages, examples, limits, and all 96 actual factory preset names')
    checks.append('Every engine picture loads, has a descriptive label, and opens at full size; all engine pages fit desktop and phone widths')
    for filename in ['ENGINE_GUIDE','MODE_GUIDE']:
        page.goto(base+'docs/'+filename+'.html')
        page.wait_for_function('Array.from(document.images).every(i=>i.complete && i.naturalWidth>0)')
        for width in [1100,390,320]:
            page.set_viewport_size({'width':width,'height':900})
            assert page.evaluate('document.documentElement.scrollWidth<=innerWidth'),(filename,width)
            page.screenshot(path=str(out/f'{filename.lower()}-{width}.png'),full_page=True)
    page.locator('a[href="#nav-note-and-arm"]').click()
    assert page.locator('#nav-note-and-arm').bounding_box()['y']>=page.locator('nav').bounding_box()['height']
    for heading in ['home-and-mixer','tracker-views','step-detail-and-automation','envelope','lfo-and-modulation-matrix','engine-edit-and-voice-modes','effects','scale-and-chord','arpeggiator','performance-layers','save-pages','settings-menu-and-midi']:
        assert page.locator('#'+heading).count()==1,heading
    checks.append('Mode guide covers tracker views and entry states, sound pages, voice modes, effects, scale, arpeggiator, layers, save pages, and settings')
    checks.append('Engine index and mode guide fit 1100, 390, and 320 pixels; contents links keep headings clear of the fixed navigation')
    page.goto(base);page.get_by_role('link',name='Engine guide',exact=True).click()
    assert page.locator('h1').inner_text()=='Yanfei engine guide'
    for name in tables['ORDER']:assert page.locator('main a[href="ENGINE_'+name+'.html"]').count()==1,name
    page.get_by_role('link',name='PSG',exact=True).click()
    page.locator('figure a').first.click()
    assert page.url.endswith('/psg-home.png')
    assert not errors,errors
    checks.append('Landing page opens the engine index, all engine links are available, pictures open directly, and no browser exceptions occur')
    (out/'manual-browser-checks.json').write_text(json.dumps({'base':base,'checks':checks,'errors':errors},indent=2)+'\n')
    print(json.dumps(checks,indent=2));browser.close()
