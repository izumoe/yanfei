#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the editor UI against its MIDI mock, optionally with host-dumped core tables.

python3 web/test_editor_ux.py URL OUT [CORE_DESC_JSON CORE_ENGINE_DESC_JSON]
The optional tables must come from the firmware. They replace only mock tables in
an intercepted test page; the shipped editor and its MIDI protocol are unchanged.
Generate both fixtures from one host build (after the usual firmware table setup):
  cc -DFELUCCA_CORE=1 -fwrapv -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src tests/descdump.c -lm -o build/core-desc
  build/core-desc > build/core-desc.json
  build/core-desc --engine-descriptors > build/core-engine-desc.json
"""
import json
import shutil
import sys
from pathlib import Path
from playwright.sync_api import sync_playwright

url=sys.argv[1];out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
core=json.loads(Path(sys.argv[3]).read_text()) if len(sys.argv)>3 else None
engine_desc=json.loads(Path(sys.argv[4]).read_text()) if len(sys.argv)>4 else None
checks=[]
with sync_playwright() as p:
    browser=p.chromium.launch(executable_path=shutil.which('chromium'),args=['--no-sandbox'])
    context=browser.new_context(viewport={'width':1560,'height':1168},locale='en-US',ignore_https_errors=url.startswith('https:'))
    def fixture(route):
        if route.request.resource_type != "document":
            route.continue_();return
        response=route.fetch();html=response.text()
        html=html.replace('makeMockDevice({ legacy:', 'makeMockDevice({ auto: false, legacy:')
        if core:
            html=html.replace('  const FM6_E = ', '  ENG.splice(0, ENG.length, ...'+json.dumps(core['ENG'])+');\n  const FM6_E = ',1)
            html=html.replace('  const P_COUNT = 111,', '  TP.splice(0, TP.length, ...'+json.dumps(core['TP'])+');\n  GP.splice(0, GP.length, ...'+json.dumps(core['GP'])+');\n  const P_COUNT = 111,',1)
            a=html.index('  const desc = (scope, id) =>',html.index('function makeMockDevice'))
            b=html.index('  const EMPTY =',a)
            html=html[:a]+'  const fixtureDescriptors = '+json.dumps(engine_desc)+';\n  const desc = (scope,id) => scope === 0 ? fixtureDescriptors[st.engine][id] : GP[id];\n'+html[b:]
        if core:
            html=html.replace('    if (st.engine === FM6_E) {', '    for (let id=0; id<P_E0; id++) { const d=fixtureDescriptors[st.engine][id]; st.p[id]=Math.max(d.min, Math.min(d.max, st.p[id])); }\n    if (st.engine === FM6_E) {',1)
        html=html.replace('applyLang();\nshowTab(tab);','window.__editor = { get device() { return dev; }, get mock() { return mock; }, isBusy: () => busy };\napplyLang();\nshowTab(tab);',1)
        route.fulfill(response=response,body=html)
    path=url.split('?')[0]
    context.route(path+'*',fixture)
    page=context.new_page();errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
    page.goto(path+'?mock=1#sound')
    page.locator('#welcome-connect').click()
    page.wait_for_function('window.__editor.device?.dump && !window.__editor.isBusy()')
    def settle():
        page.wait_for_function('window.__editor.device?.dump && !window.__editor.isBusy()')
        page.wait_for_timeout(100)
    assert page.locator('#groups > .sound-column').count()==4
    assert page.locator('#groups .shape-preview').count()==2
    checks.append('Connect opens four sound columns with envelope and LFO previews')
    # Every card page must be reachable, retain a single selected tab, and expose its controls.
    for card in page.locator('#groups .group').all():
        buttons=card.locator('.page-tabs button').all()
        for button in buttons:
            button.click()
            assert button.get_attribute('aria-selected')=='true'
            target=button.get_attribute('aria-controls')
            assert page.locator('#'+target).is_visible()
        if buttons: buttons[0].click()
    checks.append('All card pages open, including LFO sync, insert effects, and voice spread')
    attack=page.locator('[data-param="0:1"] input')
    before=page.locator('[data-shape="ENV"] .shape-curve').get_attribute('d')
    attack.focus();attack.press('End');settle()
    assert page.evaluate('__editor.mock.state.p[1]')==127
    assert page.locator('[data-shape="ENV"] .shape-curve').get_attribute('d')!=before
    attack.press('Shift+ArrowLeft');settle()
    assert page.evaluate('__editor.mock.state.p[1]')==117
    box=attack.bounding_box();page.mouse.click(box['x']+box['width']*.35,box['y']+box['height']/2);settle()
    assert 30<page.evaluate('__editor.mock.state.p[1]')<60
    assert attack.get_attribute('aria-valuetext')
    checks.append('Keyboard, fine adjustment, and pointer edits reach MIDI state and update the envelope preview')
    wave=page.locator('[data-param="0:10"] select');wave.select_option('1');settle()
    tri=page.locator('[data-shape="LFO"] .shape-curve').get_attribute('d')
    wave.select_option('0');settle()
    assert page.locator('[data-shape="LFO"] .shape-curve').get_attribute('d')!=tri
    page.evaluate('__editor.mock.sim.param(0, 3, 127)');page.wait_for_timeout(200)
    assert page.locator('[data-param="0:3"] input').input_value()=='127'
    checks.append('Enum controls and device pushes update both controls and previews')
    old=page.locator('#preset').input_value();page.locator('#preset-next').click();settle()
    assert page.locator('#preset').input_value()!=old
    page.locator('#preset-prev').click();settle();assert page.locator('#preset').input_value()==old
    checks.append('Previous and next preset buttons select device presets')
    page.locator('#trackbtns button').nth(1).click();settle()
    assert page.evaluate('__editor.mock.state.sel')==1
    page.locator('#trackbtns button').nth(0).click();settle()
    checks.append('Top track buttons change the active device track')
    for option in page.locator('#engine option').all():
        value=option.get_attribute('value');name=option.inner_text()
        page.locator('#engine').select_option(value);settle()
        assert page.evaluate('__editor.mock.state.engine')==int(value)
        assert page.locator('[data-group="EDIT"] h2').inner_text().endswith(name)
        ids=page.locator('#groups .param').evaluate_all('(rows)=>rows.map(r=>r.dataset.param)')
        assert len(ids)==len(set(ids)),(name,'duplicate controls')
        if core:
            for i,d in enumerate(engine_desc[int(value)][103:111],103):
                if d['max']>d['min'] and d['label']!='-': assert '0:'+str(i) in ids,(name,i)
        if name=='PSG':
            chip=page.locator('[data-group="CHIP"] select');assert chip.count()==1
            chip.select_option('1');settle();assert page.evaluate('__editor.mock.state.p[8]')==1
        if name=='YM':
            assert page.locator('[data-group="OP ENV"]').count()==1
            assert page.locator('[data-group="OP TONE"]').count()==1
            page.screenshot(path=str(out/'ym-desktop.png'),full_page=True)
    checks.append('Every engine opens with unique controls; core tests cover all engine parameter descriptors and PSG/YM pages' if core else 'Every legacy engine opens with unique controls')
    page.locator('#engine').select_option('4' if core else '0');settle()
    page.locator('#preset').select_option('0');settle()
    for width in [1560,1024,768,390,320]:
        page.set_viewport_size({'width':width,'height':1168 if width>760 else 844})
        assert page.evaluate('document.documentElement.scrollWidth<=innerWidth'),width
        for card in page.locator('#groups .group').all():
            box=card.bounding_box();assert box['width']>100
        page.screenshot(path=str(out/f'sound-{width}.png'),full_page=True)
    checks.append('Sound layout fits 1560, 1024, 768, 390, and 320 pixel screens')
    for width in [1560,390]:
        page.set_viewport_size({'width':width,'height':1168 if width>760 else 844})
        for tab in ['sequencer','tracks','library','samples','projects','settings','fm6']:
            button=page.locator('#tabs button[data-tab="'+tab+'"]');button.click();settle()
            panel=page.locator('#p-'+tab)
            assert panel.is_visible(),tab
            assert page.evaluate('document.documentElement.scrollWidth<=innerWidth'),(tab,width)
            page.screenshot(path=str(out/f'{tab}-{width}.png'),full_page=True)
    checks.append('Sequencer, mixer, library, samples, projects, settings, and FM6 remain available on desktop and phone')
    page.locator('#tabs button[data-tab="sound"]').click();settle()
    page.locator('#theme').click();assert page.locator('html').get_attribute('data-theme')=='light'
    page.screenshot(path=str(out/'light-mobile.png'),full_page=True)
    page.reload();page.locator('#connect').click();settle()
    assert page.locator('html').get_attribute('data-theme')=='light'
    page.locator('#theme').click()
    page.locator('#lang').click();assert page.locator('html').get_attribute('lang')=='ja'
    page.locator('#lang').click()
    checks.append('Light theme persists across reloads; English and Japanese controls remain available')
    page.locator('.sound-files summary').click()
    with page.expect_download() as download:
        page.locator('#savefile').click()
    saved=download.value;saved.save_as(str(out/'exported-preset.json'))
    assert json.loads((out/'exported-preset.json').read_text())
    page.locator('#connect').click()
    assert page.locator('#welcome').is_visible()
    assert not page.locator('#p-sound').is_visible()
    assert not errors,errors
    checks.append('Preset export works; disconnect returns to the welcome screen with no browser errors')
    (out/'editor-browser-checks.json').write_text(json.dumps({'url':url,'core':bool(core),'checks':checks,'errors':errors},indent=2)+'\n')
    print(json.dumps(checks,indent=2));browser.close()
