#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check the rendered guides' pictures, navigation, and phone layout."""
import json
import shutil
import sys
from pathlib import Path
from playwright.sync_api import sync_playwright

base=sys.argv[1].rstrip('/')+'/';out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
checks=[]
with sync_playwright() as p:
    browser=p.chromium.launch(executable_path=shutil.which('chromium'),headless=True,args=['--no-sandbox'])
    page=browser.new_page(viewport={'width':1100,'height':900},ignore_https_errors=base.startswith('https:'))
    errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
    page.goto(base);page.get_by_role('link',name='Visual guide: start here').click()
    assert page.locator('h1').inner_text()=='Yanfei visual guide'
    for filename in ['VISUAL_GUIDE','USER_GUIDE']:
        page.goto(base+f'docs/{filename}.html')
        page.wait_for_function('Array.from(document.images).every(i=>i.complete && i.naturalWidth>0)')
        assert page.locator('figure img').count()>=12
        assert page.locator('figure img').evaluate_all('(imgs)=>imgs.every(i=>i.alt.length>20 && i.closest("a").href.endsWith(".png"))')
        for width in [1100,390]:
            page.set_viewport_size({'width':width,'height':900})
            assert page.evaluate('document.documentElement.scrollWidth<=innerWidth')
            page.screenshot(path=str(out/f'{filename.lower()}-{width}.png'),full_page=True)
        checks.append(f'{filename}: all pictures load, have descriptive labels, open at full size, and fit desktop and phone widths')
    page.goto(base+'docs/VISUAL_GUIDE.html')
    page.locator('a[href="#move-through-four-bars"]').click()
    assert page.locator('#move-through-four-bars').bounding_box()['y']>=page.locator('nav').bounding_box()['height']
    checks.append('Contents links keep headings visible below the persistent navigation')
    page.locator('a[href="images/guide/page4.png"]').click()
    assert page.url.endswith('/docs/images/guide/page4.png')
    checks.append('A screen picture opens directly for a closer view')
    assert not errors,errors
    (out/'guide-browser-checks.json').write_text(json.dumps({'base':base,'checks':checks},indent=2)+'\n')
    print(json.dumps(checks));browser.close()
