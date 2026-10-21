// SPDX-License-Identifier: GPL-3.0-only
// Check the optional desktop library against the actual editor importer.
import fs from 'node:fs';import vm from 'node:vm';import assert from 'node:assert/strict';
const lib=JSON.parse(fs.readFileSync(process.argv[2]||'build/core6/factory-library.json','utf8'));
const html=fs.readFileSync('web/editor.html','utf8');
const proto=html.slice(html.indexOf('/*PROTO-BEGIN*/'),html.indexOf('/*PROTO-END*/'));
const api=vm.runInNewContext(proto+';({readLibraryFile})',{setTimeout,clearTimeout,setInterval,clearInterval,console});
const ctx={keys:lib.paramLabels,engines:lib.engines,pe0:lib.pE0};
const got=api.readLibraryFile(lib,ctx);
assert.equal(got.skipped,0);assert.equal(got.patches.length,96);
for(let i=0;i<96;i++){
 const a=lib.patches[i],b=got.patches[i];
 assert.equal(a.name,b.name);assert.equal(a.engine,b.engine);
 assert.equal(JSON.stringify(a.params),JSON.stringify(b.p));
 if(a.fm6)assert.equal(JSON.stringify(a.fm6),JSON.stringify(b.fm6));
}
console.log('factory library: all 96 import without remapping or loss, including every FM6 OWN patch');
