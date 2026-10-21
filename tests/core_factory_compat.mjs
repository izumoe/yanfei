// SPDX-License-Identifier: GPL-3.0-only
// Preserve the original 36 factory sounds against a prior emulator snapshot.
import fs from 'node:fs';
const [oldPath,newPath,all]=process.argv.slice(2);
if(!oldPath||!newPath)throw Error('usage: node tests/core_factory_compat.mjs OLD.wasm NEW.wasm');
const oldBytes=fs.readFileSync(oldPath),newBytes=fs.readFileSync(newPath);
async function render(bytes,e,p){
 const {instance}=await WebAssembly.instantiate(bytes,{}),x=instance.exports;
 x._initialize();x.web_nor_erase();x.web_boot();
 const advance=n=>{for(let k=0;k<n;k+=128)x.web_render(Math.min(128,n-k));};
 advance(44100);x.web_test_core_sound(e,p);advance(4410);
 for(const n of e===10?[36,38]:[60,67])x.web_midi(0x90,n,100);
 const samples=[];
 for(let block=0;block<768;block++){
  if(block===256)for(const n of e===10?[36,38]:[60,67])x.web_midi(0x80,n,0);
  x.web_render(128);
  const l=new Float32Array(x.memory.buffer,x.web_out_l(),128),r=new Float32Array(x.memory.buffer,x.web_out_r(),128);
  for(let i=0;i<128;i++)samples.push(l[i],r[i]);
 }
 return new Float32Array(samples);
}
const perEngine=all==='--all'?8:3;
for(const e of [0,3,4,5,6,12,13,7,2,8,9,10])for(let p=0;p<perEngine;p++){
 const a=await render(oldBytes,e,p),b=await render(newBytes,e,p);
 for(let i=0;i<a.length;i++)if(a[i]!==b[i])throw Error(`factory sound changed: engine ${e} preset ${p}, sample ${i}: ${a[i]} != ${b[i]}`);
}
console.log(`factory compatibility: ${perEngine*12} presets bit identical to ${oldPath} through note-on/release/FX (98,304 frames each)`);
