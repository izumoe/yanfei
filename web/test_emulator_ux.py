#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise browser viewing controls with the actual AudioWorklet and WASM.
python3 web/test_emulator_ux.py http://127.0.0.1:8790/ build/visual-guide
"""
import json
import shutil
import sys
from pathlib import Path
from playwright.sync_api import sync_playwright

url=sys.argv[1];out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
with sync_playwright() as p:
    browser=p.chromium.launch(executable_path=shutil.which('chromium'),headless=True,args=['--no-sandbox'])
    page=browser.new_page(viewport={'width':1100,'height':1000},ignore_https_errors=url.startswith('https:'))
    errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
    page.add_init_script('''window.inputMessages=[];
      const original=MessagePort.prototype.postMessage;
      MessagePort.prototype.postMessage=function(m,...rest){
        if(m && ['keys','buttons','enc'].includes(m.type)) window.inputMessages.push({...m});
        return original.call(this,m,...rest);
      };''')
    page.goto(url);page.locator('#go').click()
    page.wait_for_function('document.querySelector("#start").hidden',timeout=30000)
    page.wait_for_timeout(1300)
    page.locator('.hit[aria-label="SEQ"]').click();page.wait_for_timeout(400)
    checks=[]
    page.evaluate('inputMessages.length=0');page.locator('#tracker-labels').check()
    assert page.locator('.key-label').count()==27
    assert page.evaluate('inputMessages.length')==0
    checks.append('Key labels show all 27 positions without sending notes or changing firmware state')
    # The checkbox retains focus. Computer shortcuts still work after a click.
    page.keyboard.down('r');page.keyboard.up('r')
    assert page.evaluate('inputMessages.filter(m=>m.type==="keys").slice(-2).map(m=>m.mask)')==[1<<21,0]
    checks.append('Computer shortcuts work after the label checkbox is used')
    # A layout rebuild releases an active pointer instead of stranding a note.
    box=page.locator('.hit[aria-label="C4"]').bounding_box()
    page.mouse.move(box['x']+box['width']/2,box['y']+box['height']/2);page.mouse.down()
    page.set_viewport_size({'width':390,'height':844});page.mouse.up()
    assert page.evaluate('inputMessages.filter(m=>m.type==="keys").at(-1).mask')==0
    page.set_viewport_size({'width':1100,'height':1000})
    checks.append('Resizing during a pointer press releases the key')
    page.locator('.hit[aria-label="SHIFT (D5)"]').click(modifiers=['Shift'])
    page.locator('.hit[aria-label="OPTION (F5)"]').click(modifiers=['Shift'])
    assert page.evaluate('inputMessages.filter(m=>m.type==="keys").at(-1).mask')==(1<<21)|(1<<24)
    assert page.locator('.hit[aria-label="SHIFT (D5)"]').get_attribute('aria-pressed')=='true'
    page.set_viewport_size({'width':390,'height':844})
    assert page.locator('.hit[aria-label="SHIFT (D5)"]').get_attribute('aria-pressed')=='true'
    page.locator('#release-controls').click()
    assert page.evaluate('inputMessages.filter(m=>m.type==="keys").at(-1).mask')==0
    assert page.locator('#panel .latched').count()==0
    checks.append('Two held modifiers survive a layout change and release together')
    page.locator('.hit[aria-label="SHIFT (D5)"]').click(modifiers=['Shift'])
    page.locator('#tracker-labels').uncheck()
    assert page.evaluate('inputMessages.filter(m=>m.type==="keys").at(-1).mask')==0
    assert page.locator('.key-label').count()==0
    checks.append('Turning off labels releases held tracker modifiers')
    page.locator('#tracker-labels').check()
    page.locator('#screen-enlarge').click()
    assert page.locator('#screen-dialog').evaluate('(e)=>e.open')
    before=page.locator('#screen-large').evaluate('(e)=>e.toDataURL()')
    page.keyboard.press('Equal');page.wait_for_timeout(400) # panel REC
    assert page.evaluate('document.querySelector("#screen-large").toDataURL()===document.querySelector("#screen").toDataURL()')
    assert before!=page.locator('#screen-large').evaluate('(e)=>e.toDataURL()')
    assert page.evaluate('document.documentElement.scrollWidth<=innerWidth')
    page.screenshot(path=str(out/'emulator-mobile-screen.png'))
    page.keyboard.press('Escape');assert not page.locator('#screen-dialog').evaluate('(e)=>e.open')
    checks.append('Enlarged live screen follows firmware updates, fits a phone and closes with Escape')
    page.screenshot(path=str(out/'emulator-mobile.png'),full_page=True)
    page.set_viewport_size({'width':1100,'height':1000})
    page.keyboard.press('Equal');page.wait_for_timeout(200) # return to NAV
    page.locator('#device').screenshot(path=str(out/'panel-map.png'))
    box=page.locator('#device').bounding_box();scale=box['width']/1000
    page.screenshot(path=str(out/'keyboard-map.png'),clip={'x':box['x']+36*scale,'y':box['y']+332*scale,'width':928*scale,'height':256*scale})
    page.screenshot(path=str(out/'emulator-desktop.png'),full_page=True)
    assert not errors,errors
    checks.append('No browser exceptions during controls, resizing or live audio')
    (out/'emulator-ux-checks.json').write_text(json.dumps({'url':url,'checks':checks},indent=2)+'\n')
    print(json.dumps(checks));browser.close()
