#pragma once

// SPDX-License-Identifier: CC-BY-NC-4.0
// Shared setup/settings network picker by axelaberg91.
#include <pgmspace.h>

static const char WIFI_PICKER_JS[] PROGMEM = R"WIFIPICKER(
(function(){
'use strict';
// The device serves one HTTP client at a time. Start each timeout when its turn begins.
window.WatchsyncRequest=function(){
var tail=Promise.resolve(),pending=0;
return function(url,options){
if(pending>=6)return Promise.reject(new Error('The device request queue is full. Wait a moment and try again.'));
++pending;
var result=tail.then(function(){
var controller=new AbortController(),timeout=setTimeout(function(){controller.abort();},12000);
var request=Object.assign({},options||{});request.signal=controller.signal;request.cache='no-store';
return fetch(url,request).then(function(response){
var json=(response.headers.get('Content-Type')||'').indexOf('application/json')>=0;
return (json?response.json():response.text()).then(function(data){
if(!response.ok)throw new Error(json?data.message||'Request failed ('+response.status+').':data||'Request failed ('+response.status+').');
return data;
});
}).catch(function(error){
if(controller.signal.aborted){var timeoutError=new Error('The device did not respond in time. Check its status before retrying.');timeoutError.name='TimeoutError';throw timeoutError;}
if(error.name==='TypeError'){var networkError=new Error('Cannot reach the device. Check your Wi-Fi connection and try again.');networkError.name='NetworkError';throw networkError;}
throw error;
}).finally(function(){clearTimeout(timeout);});
}).finally(function(){--pending;});
tail=result.catch(function(){});
return result;
};
};

window.WatchsyncWifiPicker=function(options){
var select=document.getElementById(options.select),input=document.getElementById(options.input);
var manual=document.getElementById(options.manual),refresh=document.getElementById(options.refresh);
var status=document.getElementById(options.status),networks=[],active=false,locked=false,timer=null,searched=false,manualChosen=false,startedAt=0,retries=0;
function message(text,error){status.textContent=text;status.className=error?'network-status error':'network-status';}
function controls(){select.disabled=locked;refresh.disabled=locked||active;refresh.textContent=active?'Searching…':'Refresh networks';}
function choose(){
var selected=select.value,network=selected.indexOf('network-')===0?networks[Number(selected.slice(8))]:null;
manualChosen=selected==='manual';
manual.hidden=selected!=='manual';input.required=selected==='manual';select.required=selected!=='manual';
if(network)input.value=network.ssid;
if(options.onSelect)options.onSelect(network);
}
function render(){
var wanted=input.value,wasManual=manualChosen,match=-1;
select.replaceChildren();
var placeholder=document.createElement('option');placeholder.value='';placeholder.textContent='Select a network';placeholder.disabled=true;select.appendChild(placeholder);
networks.forEach(function(network,index){
var option=document.createElement('option');option.value='network-'+index;
var signal=network.rssi>=-60?'Strong':network.rssi>=-75?'Good':'Weak';
option.textContent=network.ssid+' · '+signal+(network.open?' · Open':'');select.appendChild(option);
if(network.ssid===wanted)match=index;
});
if(wanted&&match<0&&!wasManual){var saved=document.createElement('option');saved.value='saved';saved.textContent=wanted+' · Saved network';select.appendChild(saved);}
var custom=document.createElement('option');custom.value='manual';custom.textContent='Hidden network / enter name';select.appendChild(custom);
select.value=wasManual?'manual':match>=0?'network-'+match:wanted?'saved':!networks.length&&searched?'manual':'';
manual.hidden=select.value!=='manual';input.required=select.value==='manual';select.required=select.value!=='manual';
controls();
}
function finish(){active=false;clearTimeout(timer);controls();if(options.onBusy)options.onBusy(false);}
function consume(data){
if(data.state==='scanning'){if(Date.now()-startedAt>=30000){fail(new Error('Network search took too long. Refresh or enter the network name manually.'));return;}message('Searching for 2.4 GHz networks…');if(!document.hidden)timer=setTimeout(poll,1000);return;}
if(data.state==='error'){fail(new Error(data.message||'Could not search. Refresh or enter the network name manually.'));return;}
searched=true;networks=Array.isArray(data.networks)?data.networks.filter(function(n){return typeof n.ssid==='string'&&n.ssid.length;}):[];
finish();render();message(networks.length?'Choose your network, or enter a hidden network manually.':'No networks found. Refresh or enter the network name manually.');
}
function fail(error){finish();if(!networks.length){searched=true;manualChosen=true;render();}message(error.message||'Could not search. Refresh or enter the network name manually.',true);}
function recover(error){
if(active&&retries<1&&Date.now()-startedAt<15000&&(error.name==='TimeoutError'||error.name==='NetworkError')){++retries;message('Waiting for the device. The network search may still be running…');if(!document.hidden)timer=setTimeout(poll,1500);return;}
fail(error);
}
function poll(){clearTimeout(timer);if(!active||document.hidden)return;options.request('/api/wifi/networks').then(consume).catch(recover);}
function load(cached){
if(active||locked||!options.token())return;active=true;startedAt=Date.now();retries=0;controls();if(options.onBusy)options.onBusy(true);message('Searching for 2.4 GHz networks…');
var form=new URLSearchParams();form.set('token',options.token());
options.request('/api/wifi/networks',cached?undefined:{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:form.toString()}).then(consume).catch(recover);
}
select.addEventListener('change',choose);refresh.addEventListener('click',function(){load(false);});
document.addEventListener('visibilitychange',function(){clearTimeout(timer);if(!document.hidden&&active)poll();});
render();
return {load:load,setSSID:function(value){manualChosen=false;input.value=value||'';render();},lock:function(value){locked=!!value;controls();},isBusy:function(){return active;}};
};
})();
)WIFIPICKER";
