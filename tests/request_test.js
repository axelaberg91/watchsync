// SPDX-License-Identifier: CC-BY-NC-4.0
// Slow-device and interrupted-network regression checks by axelaberg91.
'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const header = fs.readFileSync(path.join(__dirname,'../watchsync/wifi_picker.h'),'utf8');
const script = header.split('R"WIFIPICKER(')[1].split(')WIFIPICKER"')[0];
const settle = async () => { for(let i=0;i<50;++i) await Promise.resolve(); };
const response = (data,status=200,json=true) => ({ok:status<400,status,headers:{get:()=>json?'application/json':'text/plain'},json:async()=>data,text:async()=>data});
class Element {
  constructor() { this.value='';this.listeners={};this.children=[]; }
  addEventListener(name,fn) { this.listeners[name]=fn; }
  replaceChildren() { this.children=[]; }
  appendChild(child) { this.children.push(child); }
}
function harness() {
  let now=0,sequence=0,active=0,peak=0;
  const timers=new Map(),calls=[],elements=new Map();
  const context=vm.createContext({AbortController,URLSearchParams,Date:{now:()=>now},
    document:{hidden:false,addEventListener(){},createElement:()=>new Element(),getElementById(id){if(!elements.has(id))elements.set(id,new Element());return elements.get(id);}},
    setTimeout(fn,delay){timers.set(++sequence,{fn,delay,due:now+delay});return sequence;},clearTimeout(id){timers.delete(id);},
    fetch:(url,options)=>new Promise((resolve,reject)=>{
      ++active;peak=Math.max(peak,active);let done=false;
      const finish=(fn,value)=>{if(done)return;done=true;--active;fn(value);};
      calls.push({url,options,resolve:data=>finish(resolve,data),reject:error=>finish(reject,error)});
      options.signal.addEventListener('abort',()=>{const error=new Error('signal is aborted without reason');error.name='AbortError';finish(reject,error);});
    })});
  context.window=context;vm.runInContext(script,context);
  async function advance(ms) {
    const target=now+ms;
    for(;;){
      const due=[...timers].filter(([,t])=>t.due<=target).sort((a,b)=>a[1].due-b[1].due)[0];
      if(!due)break;
      now=due[1].due;timers.delete(due[0]);due[1].fn();await settle();
    }
    now=target;await settle();
  }
  return {context,timers,calls,elements,advance,peak:()=>peak};
}
async function requests() {
  const h=harness(),api=h.context.WatchsyncRequest();
  const first=api('/slow'),second=api('/next');await settle();
  assert.equal(h.calls.length,1);assert.equal(h.peak(),1);
  assert.equal([...h.timers.values()][0].delay,12000);
  await h.advance(6000);h.calls[0].resolve(response({ok:true}));await first;await settle();
  assert.equal(h.calls.length,2,'A slow response must complete before the next request starts');
  assert.equal([...h.timers.values()][0].due,18000,'Queue wait must not consume a request timeout');
  h.calls[1].resolve(response('OK',200,false));assert.equal(await second,'OK');assert.equal(h.timers.size,0);

  const slow=api('/timeout');const timeoutCheck=assert.rejects(slow,error=>error.name==='TimeoutError'&&error.message.includes('did not respond in time')&&!error.message.includes('aborted'));
  const after=api('/after-timeout');await settle();await h.advance(12000);await timeoutCheck;
  assert.equal(h.calls.at(-1).url,'/after-timeout');h.calls.at(-1).resolve(response({recovered:true}));await after;

  const failed=api('/disconnected');const networkCheck=assert.rejects(failed,error=>error.name==='NetworkError'&&error.message.includes('Wi-Fi'));
  await settle();h.calls.at(-1).reject(new TypeError('Failed to fetch'));await networkCheck;
  const rejected=api('/server-error');const serverCheck=assert.rejects(rejected,/Not enough free memory/);
  await settle();h.calls.at(-1).resolve(response({message:'Not enough free memory'},503));await serverCheck;
  assert.equal(h.timers.size,0);assert.equal(h.peak(),1);

  const bounded=harness(),boundedApi=bounded.context.WatchsyncRequest();
  const results=Promise.allSettled(Array.from({length:9},(_,i)=>boundedApi('/'+i)));await settle();
  for(let i=0;i<6;++i){assert.equal(bounded.calls.length,i+1);bounded.calls[i].resolve(response({ok:true}));await settle();}
  const outcomes=await results;assert.equal(outcomes.filter(r=>r.status==='fulfilled').length,6);
  assert.equal(outcomes.filter(r=>r.status==='rejected').length,3);assert.equal(bounded.timers.size,0);assert.equal(bounded.peak(),1);
  console.log('PASS requests: 6-second responses, per-request deadlines, serialization, recovery, bounded queue, timer cleanup');
}
async function pickerRecovery() {
  const h=harness();let attempts=0;const methods=[];
  const api=async(url,options)=>{
    methods.push(options?.method||'GET');++attempts;
    if(attempts===1){const error=new Error('The device did not respond in time.');error.name='TimeoutError';throw error;}
    return {state:'ready',networks:[{ssid:'Example network',rssi:-40,open:false}]};
  };
  const picker=new h.context.WatchsyncWifiPicker({select:'network',input:'ssid',manual:'manual',refresh:'refresh',status:'status',token:()=> 'token',request:api});
  picker.load();await settle();assert(picker.isBusy());await h.advance(1500);
  assert(!picker.isBusy());assert.deepEqual(methods,['POST','GET'],'A lost scan acknowledgement is recovered by polling, not repeated writes');
  assert(h.elements.get('network').children.some(option=>option.textContent.includes('Example network')));assert.equal(h.timers.size,0);

  const broken=harness();let failures=0;
  const failedPicker=new broken.context.WatchsyncWifiPicker({select:'network',input:'ssid',manual:'manual',refresh:'refresh',status:'status',token:()=> 'token',request:async()=>{++failures;const error=new Error('Cannot reach the device.');error.name='NetworkError';throw error;}});
  failedPicker.load();await settle();await broken.advance(1500);assert(!failedPicker.isBusy());assert.equal(failures,2);
  assert.equal(broken.elements.get('manual').hidden,false);assert.equal(broken.elements.get('ssid').required,true);assert.equal(broken.timers.size,0);

  const stalled=harness();const stalledPicker=new stalled.context.WatchsyncWifiPicker({select:'network',input:'ssid',manual:'manual',refresh:'refresh',status:'status',token:()=> 'token',request:async()=>({state:'scanning'})});
  stalledPicker.load();await settle();await stalled.advance(31000);assert(!stalledPicker.isBusy());assert.equal(stalled.timers.size,0);
  const cached=harness(),cacheMethods=[];
  const cachedPicker=new cached.context.WatchsyncWifiPicker({select:'network',input:'ssid',manual:'manual',refresh:'refresh',status:'status',token:()=> 'token',request:async(url,options)=>{cacheMethods.push(options?.method||'GET');return {state:'ready',networks:[]};}});
  cachedPicker.load(true);await settle();assert.deepEqual(cacheMethods,['GET']);
  cached.elements.get('refresh').listeners.click();await settle();assert.deepEqual(cacheMethods,['GET','POST']);
  console.log('PASS network picker: lost acknowledgement, bounded retry, stalled scan deadline, visible manual fallback');
}
async function main() {
  await requests();await pickerRecovery();
  const source=fs.readFileSync(path.join(__dirname,'../watchsync/wifi_config.h'),'utf8');
  const start=source.split('void startScan()')[1].split('void serviceScan()')[0];
  assert(start.includes('scanQueued_ = true'));assert(!start.includes('WiFi.scanNetworks('),'Never start an off-channel scan inside the HTTP acknowledgement handler');
}
main().catch(error=>{console.error(error);process.exitCode=1;});
