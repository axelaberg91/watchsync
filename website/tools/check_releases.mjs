// SPDX-License-Identifier: CC-BY-NC-4.0
// Versioned download compatibility and URL-scope checks by axelaberg91.
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import vm from 'node:vm';
const script=await readFile(new URL('../site/assets/releases.js',import.meta.url),'utf8');
class Element {
  constructor(tag){this.tag=tag;this.children=[];this.style={};this.hidden=false;this.textContent='';}
  append(...children){this.children.push(...children);}
  replaceChildren(...children){this.children=children;}
  get lastChild(){return this.children.at(-1);}
}
async function page(releases,language='en'){
  const list=new Element('div'),empty=new Element('div'),heading=new Element('h3'),note=new Element('p');
  empty.querySelector=selector=>selector==='h3'?heading:note;
  await vm.runInNewContext(script,{URL,document:{documentElement:{lang:language},currentScript:{src:'https://watchsync.example/assets/releases.js'},querySelector:()=>empty,getElementById:()=>list,createElement:tag=>new Element(tag)},fetch:async()=>({ok:true,json:async()=>({schemaVersion:1,repository:'axelaberg91/watchsync',releases})})});
  return {list,empty,heading};
}
const make=(version,file)=>({version,target:'M5 Atom Lite (classic ESP32), GPIO25',partitionScheme:'min_spiffs',size:1158592,sha256:'b'.repeat(64),summary:'Release',summarySv:'Release',date:'2026-10-29',binaryUrl:`https://github.com/axelaberg91/watchsync/releases/download/v${version}/${file}`,notesUrl:`https://github.com/axelaberg91/watchsync/releases/tag/v${version}`});
for(const language of ['en','sv']){
  const p=await page([make('1.12.2','watchsync-1.12.2.bin')],language);
  assert(p.empty.hidden);
  const first=p.list.children[0].children.find(x=>x.className==='actions').children[0];
  assert(first.href.endsWith('/watchsync-1.12.2.bin'));
  assert(first.textContent.endsWith('watchsync-1.12.2.bin'));
  assert.equal(p.list.children.length,1);
}
for(const change of [r=>({...r,binaryUrl:r.binaryUrl.replace('1.12.2.bin','1.12.3.bin')}),r=>({...r,binaryUrl:r.binaryUrl+'?download=1'}),r=>({...r,binaryUrl:r.binaryUrl.replace('github.com','example.invalid')}),r=>({...r,binaryUrl:r.binaryUrl.replace('/watchsync/','/another-project/')}),r=>({...r,notesUrl:r.notesUrl.replace('v1.12.2','v1.12.3')})]){
  const p=await page([change(make('1.12.2','watchsync-1.12.2.bin'))]);
  assert(!p.empty.hidden);assert.equal(p.list.children.length,0);assert.equal(p.heading.textContent,'Release information unavailable');
}
const pending=await page([]);assert(!pending.empty.hidden);assert.equal(pending.list.children.length,0);
console.log('Release cards accept versioned assets in both languages, preserve the empty fallback and reject wrong repositories, versions, hosts and queries.');
