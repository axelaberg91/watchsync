// SPDX-License-Identifier: CC-BY-NC-4.0
// Browser-script regression checks by axelaberg91. No external packages required.
'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const source = name => fs.readFileSync(path.join(__dirname, '..', 'watchsync', name), 'utf8');
const raw = (name, delimiter) => source(name).split('R"' + delimiter + '(')[1].split(')' + delimiter + '"')[0];
const ui = raw('web_ui.h', 'WATCHSYNC');
const setup = raw('wifi_config.h', 'WIFISETUP');
const picker = raw('wifi_picker.h', 'WIFIPICKER');
const inlineScript = html => [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m => m[1]).join('\n');
class Element {
  constructor(attributes = '') {
    this.value = /\bvalue="([^"]*)"/.exec(attributes)?.[1] || '';
    this.hidden = /\bhidden\b/.test(attributes);
    this.disabled = /\bdisabled\b/.test(attributes);
    this.children = []; this.listeners = {}; this.textContent = '';
  }
  addEventListener(name, callback) { this.listeners[name] = callback; }
  replaceChildren() { this.children = []; }
  appendChild(child) { this.children.push(child); }
  focus() {}
  select() {}
}
async function page(html, status, wifiState = 'idle') {
  const elements = new Map([...html.matchAll(/<[^>]+\bid="([^"]+)"[^>]*>/g)].map(m => [m[1], new Element(m[0])]));
  const timers = new Map(), calls = []; let timerId = 0;
  const network = {ssid:'Example network',rssi:-45,open:false};
  const document = {hidden:false, getElementById(id) { assert(elements.has(id), 'Missing UI element: '+id); return elements.get(id); }, createElement() { return new Element(); }, addEventListener() {}};
  const context = vm.createContext({document, URLSearchParams, AbortController, performance:{now:()=>0},
    setTimeout(fn) { timers.set(++timerId, fn); return timerId; }, clearTimeout(id) { timers.delete(id); },
    fetch: async (url, options) => { calls.push({url, options}); return {ok:true, headers:{get:()=> url==='/api/diagnostics'?'text/plain':'application/json'}, text:async()=> 'watchsync test\nWi-Fi: connected\nConfigured NTP servers: pool.ntp.org, time.nist.gov', json:async()=> {
      if (url==='/api/status') return status;
      if (url==='/api/wifi') return {token:'setup-token',state:wifiState,ssid:'',ip:'192.0.2.12',message:'Connection status',hostname:status.hostname,nextHostname:status.nextHostname};
      if (url==='/api/wifi/networks') return {state:'ready',networks:[network]};
      if (url==='/api/wifi/next') return {token:'settings-token',canEdit:true,currentSSID:'Example network',nextSSID:'Example network'};
      if (url==='/api/device') { status={...status,nextHostname:new URLSearchParams(options.body).get('hostname'),hostnamePending:true}; return status; }
      if (url==='/api/device/reset') return {message:'Factory reset saved.'};
      throw new Error('Unexpected request '+url);
    }}; }});
  context.window = context;
  vm.runInContext(picker, context, {filename:'wifi-picker.js'});
  vm.runInContext(inlineScript(html), context, {filename:'interface.js'});
  const settle = async () => { for(let i=0;i<30;++i) await Promise.resolve(); };
  await settle();
  return {elements,timers,settle,calls};
}
async function main() {
  for (const [html, id] of [[setup,'hostname'],[ui,'deviceHostname']]) {
    const input=[...html.matchAll(/<input[^>]+>/g)].map(m=>m[0]).find(tag=>tag.includes('id="'+id+'"'));
    const pattern=new RegExp('^(?:'+/pattern="([^"]+)"/.exec(input)[1]+')$','v');
    for (const name of ['watchsync','watchsync-bedroom','1','a'.repeat(32)]) assert(pattern.test(name));
    for (const name of ['','-bad','bad-','bad.local','bad_name','Bad','a'.repeat(33)]) assert(!pattern.test(name));
  }
  const base = {ntpServers:'pool.ntp.org, time.nist.gov',version:'test',token:'status-token',hostname:'watchsync',nextHostname:'watchsync',hostnamePending:false,ip:'192.0.2.12',state:'transmitting',requested:true,active:true,scheduleEnabled:false,scheduleStart:0,scheduleEnd:300,
    sessionRequested:false,sessionPending:false,sessionRemainingSeconds:0,wifi:true,timeZoneName:'Central Europe',timeZone:'central-europe',
    station:3,stationName:'DCF77',pin:25,drive:2,dst:2,led:true,ledState:'green',otaEnabled:true,localTime:'2026-10-20 12:00:00',syncAgeSeconds:10,remainingSeconds:82790};
  for (const scenario of [
    {name:'always on',changes:{},visible:false},
    {name:'setup AP',changes:{ledState:'blue'},visible:false},
    {name:'healthy standby',changes:{state:'scheduled',active:false,scheduleEnabled:true,ledState:'purple'},visible:true},
    {name:'scheduled rest',changes:{state:'scheduled',active:false,scheduleEnabled:true},visible:true},
    {name:'scheduled transmission',changes:{scheduleEnabled:true},visible:false},
    {name:'manual pause',changes:{state:'paused',requested:false,active:false,scheduleEnabled:true},visible:false},
    {name:'expired time',changes:{state:'stale',active:false,scheduleEnabled:true},visible:false},
    {name:'queued session',changes:{state:'aligning',active:false,sessionRequested:true,sessionPending:true},visible:true},
    {name:'active session',changes:{sessionRequested:true,sessionRemainingSeconds:3600},visible:true}
  ]) {
    const {elements,timers} = await page(ui,{...base,...scenario.changes});
    assert.equal(!elements.get('session').hidden,scenario.visible,scenario.name);
    if(scenario.changes.sessionRequested) assert.equal(elements.get('session').textContent,'Stop one-hour pass');
    assert.equal(elements.has('settingPin'),false);
    assert.equal(elements.get('antennaPin').textContent,'GPIO25');
    assert.equal(elements.get('deviceIP').textContent,'192.0.2.12');
    assert.equal(elements.get('firmwareUpdateLink').hidden,false);
    if(scenario.changes.ledState==='blue') assert.equal(elements.get('ledStatus').textContent,'Blue · Wi-Fi setup');
    if(scenario.changes.ledState==='purple') assert.equal(elements.get('ledStatus').textContent,'Purple · Standby');
    assert(timers.size<=2,'Bound clock and status timers');
  }
  const ap = await page(setup,base);
  const noOTA = await page(ui,{...base,otaEnabled:false});
  assert.equal(noOTA.elements.get('firmwareUpdateLink').hidden,true);
  assert(ap.elements.get('setupNetwork').children.some(option=>option.textContent.includes('Example network')),'Initial setup loads scanned networks');
  assert.equal(ap.elements.get('save').disabled,false);
  const home = await page(ui,base);
  const details = home.elements.get('networkSettings'); details.open=true; details.listeners.toggle.call(details);
  await home.settle();
  assert(home.elements.get('wifiNetwork').children.some(option=>option.textContent.includes('Example network')),'Normal settings load scanned networks');
  assert.equal(home.elements.get('wifiSaveRestart').disabled,false);
  const connected = await page(setup,base,'connected');
  assert.equal(connected.elements.get('destinations').hidden,false);
  assert.equal(connected.elements.get('destination').href,'http://192.0.2.12');
  const renamedSetup = await page(setup,{...base,hostname:'watchsync-bedroom',nextHostname:'watchsync-bedroom'},'connected');
  assert.equal(renamedSetup.elements.get('hostname').value,'watchsync-bedroom');
  assert.equal(renamedSetup.elements.get('hostnameDestination').href,'http://watchsync-bedroom.local');
  const renamed = await page(ui,{...base,hostname:'watchsync-office',nextHostname:'watchsync-office'});
  assert.equal(renamed.elements.get('deviceAddress').href,'http://watchsync-office.local');
  renamed.elements.get('deviceHostname').value='watchsync-bedroom';
  renamed.elements.get('hostnameForm').listeners.input();
  assert.equal(renamed.elements.get('deviceAddressPreview').textContent,'http://watchsync-bedroom.local');
  renamed.elements.get('hostnameForm').listeners.submit({preventDefault(){},submitter:{id:'hostnameSave'}});
  await renamed.settle();
  const savedName = renamed.calls.find(call=>call.url==='/api/device');
  assert.equal(new URLSearchParams(savedName.options.body).get('hostname'),'watchsync-bedroom');
  assert.equal(new URLSearchParams(savedName.options.body).get('restart'),'0');
  assert.equal(new URLSearchParams(savedName.options.body).get('token'),'status-token');
  assert.equal(renamed.elements.get('hostnamePending').hidden,false);
  assert.equal(renamed.elements.get('deviceAddress').href,'http://watchsync-office.local','Keep the active address until reboot');
  renamed.elements.get('hostnameForm').listeners.submit({preventDefault(){},submitter:{id:'hostnameRestart'}});
  await renamed.settle();
  assert(renamed.elements.get('deviceRestartHelp').textContent.includes('http://watchsync-bedroom.local'));
  const reset = await page(ui,base);
  reset.elements.get('factoryReset').listeners.click();
  assert.equal(reset.elements.get('factoryResetForm').hidden,false);
  assert.equal(reset.elements.get('factoryResetConfirm').disabled,true);
  reset.elements.get('factoryResetForm').listeners.submit({preventDefault(){}});
  await reset.settle();
  assert(!reset.calls.some(call=>call.url==='/api/device/reset'),'No reset without explicit confirmation');
  reset.elements.get('factoryResetCancel').listeners.click();
  assert.equal(reset.elements.get('factoryResetForm').hidden,true);
  reset.elements.get('factoryReset').listeners.click();
  reset.elements.get('resetConfirmation').value='RESET';
  reset.elements.get('resetConfirmation').listeners.input();
  assert.equal(reset.elements.get('factoryResetConfirm').disabled,false);
  reset.elements.get('factoryResetForm').listeners.submit({preventDefault(){}});
  await reset.settle();
  const resetCall=reset.calls.find(call=>call.url==='/api/device/reset');
  assert.equal(new URLSearchParams(resetCall.options.body).get('confirm'),'RESET');
  assert.equal(new URLSearchParams(resetCall.options.body).get('token'),'status-token');
  assert(reset.elements.get('deviceRestartHelp').textContent.includes('192.168.4.1'));
  assert.equal(reset.elements.get('factoryResetConfirm').disabled,true);
  const offline=await page(ui,{...base,wifi:false});
  assert.equal(offline.elements.get('deviceIP').textContent,'Not connected');
  assert(ui.includes('<summary>Network settings</summary>'));
  assert(ui.includes('<summary>Radio/time settings</summary>'));
  assert(!ui.includes('<summary>Device settings</summary>'));
  const networkPanel=ui.split('id="networkSettings"')[1].split('</details>')[0];
  assert(networkPanel.includes('id="hostnameForm"')&&networkPanel.includes('id="deviceIP"')&&networkPanel.includes('id="deviceAddress"'));
  assert(!networkPanel.includes('id="factoryReset"'));
  const radioPanel=ui.split('id="radioSettings"')[1].split('</details>')[0];
  assert(!radioPanel.includes('id="firmwareUpdateLink"'));
  assert(ui.indexOf('id="factoryResetHeading"')>ui.indexOf('<summary>Help</summary>'));
  assert(setup.includes('<h1>Network settings</h1>'));
  assert(setup.includes('href="http://watchsync.local"'));
  assert(ui.includes('If it is already transmitting, no extra action is needed.'));
  const diagnostics=await page(ui,base);
  assert.equal(diagnostics.elements.get('ntpServers').textContent,base.ntpServers);
  diagnostics.elements.get('copyDiagnostics').listeners.click();await diagnostics.settle();
  assert.equal(diagnostics.elements.get('diagnosticsText').hidden,false);
  assert(diagnostics.elements.get('diagnosticsText').value.includes('pool.ntp.org'));
  assert(diagnostics.elements.get('diagnosticsFeedback').textContent.includes('copy the report'));
  const setupCache=await page(setup,base);
  assert(setupCache.calls.filter(c=>c.url==='/api/wifi/networks').every(c=>!c.options.method||c.options.method==='GET'));
  assert(!ui.includes('<h1>Transmitter</h1>'));
  console.log('PASS interface: existing controls, custom setup address, deferred hostname change, confirmed factory reset, bounded timers');
}
main().catch(error => { console.error(error); process.exitCode=1; });
