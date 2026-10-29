// SPDX-License-Identifier: CC-BY-NC-4.0
// Firmware upload interface checks by axelaberg91. No external packages required.
'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const raw=(name,delimiter)=>fs.readFileSync(path.join(__dirname,'..','watchsync',name),'utf8').split('R"'+delimiter+'(')[1].split(')'+delimiter+'"')[0];
const html=raw('ota_ui.h','OTAUPDATE'),picker=raw('wifi_picker.h','WIFIPICKER');
class Element {
  constructor(text=''){this.value='';this.files=[];this.disabled=/\bdisabled\b/.test(text);this.hidden=/\bhidden\b/.test(text);this.listeners={};this.textContent='';}
  addEventListener(name,fn){this.listeners[name]=fn;}
}
async function page(){
  const elements=new Map([...html.matchAll(/<[^>]+\bid="([^"]+)"[^>]*>/g)].map(m=>[m[1],new Element(m[0])]));
  const timers=new Map(),requests=[];let timerId=0,now=0,xhr=null;
  let status={version:'1.9.0',ready:true,confirmed:true,maxSize:1966080,state:'ready',bootId:'old-boot',token:'control-token',message:'Choose a watchsync firmware file to update this device.'};
  let prepareError=null;
  class FakeXHR {
    constructor(){this.headers={};this.upload={};xhr=this;}
    open(method,url){this.method=method;this.url=url;}
    setRequestHeader(key,value){this.headers[key]=value;}
    send(body){this.body=body;}
  }
  class FakeDate extends Date {static now(){return now;}}
  const document={getElementById:id=>{assert(elements.has(id),id);return elements.get(id);}};
  const context=vm.createContext({document,Date:FakeDate,URLSearchParams,AbortController,XMLHttpRequest:FakeXHR,
    setTimeout(fn,delay){timers.set(++timerId,{fn,delay});return timerId;},clearTimeout(id){timers.delete(id);},addEventListener(){},
    fetch:async(url,options)=>{
      requests.push({url,options});let data,ok=true;
      if(url==='/api/firmware')data=status;
      else if(url==='/api/firmware/prepare'){data=prepareError?{message:prepareError}:{ticket:'one-use-ticket'};ok=!prepareError;}
      else throw new Error('Unexpected request '+url);
      return {ok,headers:{get:()=> 'application/json'},json:async()=>data};
    }});
  context.window=context;
  vm.runInContext(picker,context);vm.runInContext([...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m=>m[1]).join('\n'),context);
  const settle=async()=>{for(let i=0;i<60;++i)await Promise.resolve();};await settle();
  const file={name:'watchsync.bin',size:1200000};
  const submit=async(chosen=file)=>{elements.get('firmware').files=[chosen];elements.get('firmware').listeners.change();elements.get('updateForm').listeners.submit({preventDefault(){}});await settle();};
  const tick=async(delay)=>{const item=[...timers].find(([,t])=>t.delay===delay);assert(item,'Expected timer '+delay);timers.delete(item[0]);item[1].fn();await settle();};
  return {elements,timers,requests,file,settle,submit,tick,get xhr(){return xhr;},set status(value){status={...status,...value};},set prepareError(value){prepareError=value;},set now(value){now=value;}};
}
async function main(){
  {const p=await page();assert(!p.elements.get('firmware').disabled);assert(p.elements.get('update').disabled);await p.submit({name:'merged-flash.hex',size:1200000});assert(!p.xhr);assert.equal(p.requests.length,1);}
  {const p=await page();p.prepareError='Reload the update page.';await p.submit();assert(!p.xhr);assert.equal(p.elements.get('status').textContent,'Reload the update page.');assert(!p.elements.get('back').hidden);}
  {const p=await page();await p.submit();const xhr=p.xhr;assert.equal(xhr.method,'POST');assert.equal(xhr.url,'/api/firmware/upload');assert.equal(xhr.timeout,150000);assert.equal(xhr.headers['Content-Type'],'application/octet-stream');assert.equal(xhr.headers['X-Watchsync-Update'],'one-use-ticket');assert.equal(xhr.body,p.file);assert(p.elements.get('back').hidden);assert(p.elements.get('firmware').disabled);assert(!('X-Watchsync-Password' in p.requests[1].options.headers));assert(p.requests[1].options.body.includes('control-token'));xhr.upload.onprogress({lengthComputable:true,loaded:45,total:100});assert.equal(p.elements.get('progress').value,45);xhr.upload.onload();assert.equal(p.elements.get('status').textContent,'Verifying firmware…');xhr.status=200;xhr.onload();p.status={bootId:'new-boot',version:'1.10.0',state:'confirming',ready:false,confirmed:false};await p.tick(2500);assert(p.elements.get('back').hidden);p.status={state:'ready',ready:true,confirmed:true};await p.tick(2000);assert(!p.elements.get('back').hidden);assert(p.elements.get('status').textContent.includes('Running firmware v1.10.0'));assert.equal(p.timers.size,0);}
  {const p=await page();await p.submit();p.xhr.ontimeout();await p.tick(2500);assert(!p.elements.get('back').hidden);assert(p.elements.get('status').textContent.includes('current firmware is still running'));assert.equal(p.requests.filter(r=>r.url.endsWith('/prepare')).length,1);}
  {const p=await page();await p.submit();p.xhr.onerror();p.now=90001;await p.tick(2500);assert(p.elements.get('status').textContent.includes('restore the firmware over USB'));assert(p.elements.get('firmware').disabled);assert.equal(p.timers.size,0);}
  {const p=await page();await p.submit();p.xhr.status=200;p.xhr.onload();p.status={bootId:'new-boot',state:'error',ready:false,confirmed:false,message:'Startup checks failed. Restore the firmware over USB.'};await p.tick(2500);assert.equal(p.elements.get('status').textContent,'Startup checks failed. Restore the firmware over USB.');assert(p.elements.get('firmware').disabled);assert.equal(p.timers.size,0);}
  assert(!html.includes('id="password"'));
  console.log('PASS OTA interface: file checks, password-free preparation errors, dedicated upload timeout, progress, restart confirmation, interrupted response recovery, bounded timers');
}
main().catch(error=>{console.error(error);process.exitCode=1;});
