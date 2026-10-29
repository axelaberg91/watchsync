#pragma once

// SPDX-License-Identifier: CC-BY-NC-4.0
// Wi-Fi setup, fallback and deferred configuration by axelaberg91.

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <atomic>
#include "wifi_picker.h"
#include "device_config.h"

static const char WIFI_SETUP_UI[] PROGMEM = R"WIFISETUP(
<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>watchsync · Network</title>
<style>
:root{color-scheme:light}*{box-sizing:border-box}body{margin:0;background:#f4f5f6;color:#222b30;font:15px/1.5 system-ui,-apple-system,Segoe UI,sans-serif}main{max-width:540px;margin:40px auto;padding:0 18px}h1{font-size:27px;line-height:1.2;margin:28px 0 12px}p{color:#68747b;font-size:14px}.brand{font-size:20px;font-weight:700}section{background:#fff;border:1px solid #dce1e5;border-radius:10px;padding:24px;margin-top:24px}label{display:block;margin-top:16px;font-size:13px;font-weight:600}input[type=text],input[type=password]{width:100%;min-height:44px;margin-top:6px;padding:10px;border:1px solid #cbd3d9;border-radius:6px;font:inherit}input:focus-visible,button:focus-visible,a:focus-visible{outline:3px solid #8dbba8;outline-offset:3px}.show{display:flex;align-items:center;gap:8px;font-weight:400}button{width:100%;min-height:44px;margin-top:22px;border:0;border-radius:7px;background:#222b30;color:#fff;font:inherit;font-weight:600;cursor:pointer}button:disabled{opacity:.5;cursor:wait}#result{min-height:44px;margin:16px 0 0;font-size:14px}#result.error{color:#a33832}#result.ok{color:#17694b}.note{font-size:12px}#destination{display:block;margin-top:10px;color:#17694b;overflow-wrap:anywhere}footer{font-size:12px;color:#68747b;margin-top:20px}
[hidden]{display:none!important}select{width:100%;min-height:44px;margin-top:6px;padding:10px;border:1px solid #cbd3d9;border-radius:6px;background:white;font:inherit}.secondary{width:auto;margin-top:12px;background:white;color:#222b30;border:1px solid #cbd3d9;font-size:13px;padding:8px 12px}.network-status{font-size:12px;min-height:18px}.network-status.error{color:#a33832}select:focus-visible{outline:3px solid #8dbba8;outline-offset:3px}
</style></head><body><main>
<div class="brand">watchsync</div><h1>Network settings</h1>
<p>Choose the network watchsync should connect to. The network must support 2.4 GHz. Stay connected to the watchsync setup network until the device confirms the connection.</p>
<section><form id="wifiForm">
<label for="setupNetwork">Wi-Fi network</label><select id="setupNetwork" required></select>
<button class="secondary" id="scanNetworks" type="button">Refresh networks</button><p class="network-status" id="networkStatus" role="status" aria-live="polite">Loading the network list…</p>
<div id="manualNetwork" hidden><label for="ssid">Network name (SSID)</label><input id="ssid" name="ssid" type="text" maxlength="32" autocomplete="off" autocapitalize="none" spellcheck="false"></div>
<label for="hostname">Device hostname</label><input id="hostname" name="hostname" type="text" value="watchsync" minlength="1" maxlength="32" pattern="[a-z0-9](([a-z0-9]|-){0,30}[a-z0-9])?" required autocomplete="off" autocapitalize="none" spellcheck="false"><p class="note">Web address: <span id="hostnamePreview">http://watchsync.local</span>. Use a different name for each device.</p>
<label for="psk">Password (PSK)</label><input id="psk" name="psk" type="password" minlength="8" maxlength="64" autocomplete="new-password" autocapitalize="none" spellcheck="false">
<label class="show"><input id="showPassword" type="checkbox">Show password</label>
<p class="note">Leave the password empty for an open network. Settings are saved to the device once the connection succeeds.</p>
<button id="save" type="submit" disabled>Connect and save</button>
</form><p id="result" role="status" aria-live="polite">Loading connection status…</p><div id="destinations" hidden><p class="note">After setup, rejoin your normal Wi-Fi and open <a id="hostnameDestination" href="http://watchsync.local">http://watchsync.local</a>. If that address does not work, use the device IP below.</p><a id="destination"></a></div></section>
<footer>Setup network: watchsync · <a href="http://192.168.4.1">Open setup at http://192.168.4.1</a> if the sign-in window does not appear.</footer>
</main><script src="/wifi-picker.js"></script><script>
(function(){
'use strict';
var token='',busy=false,connected=false,loaded=false,loading=false,timer=null,pickerLoaded=false,picker=null;
var el=function(id){return document.getElementById(id);};
function message(text,kind){el('result').textContent=text;el('result').className=kind||'';}
var api=WatchsyncRequest();
function controls(){el('save').disabled=busy||!token||connected||(picker&&picker.isBusy());if(picker)picker.lock(busy||connected||!token);}
picker=new WatchsyncWifiPicker({select:'setupNetwork',input:'ssid',manual:'manualNetwork',refresh:'scanNetworks',status:'networkStatus',request:api,token:function(){return token;},onBusy:controls,onSelect:function(){el('psk').value='';}});
function show(s){
token=s.token||token;connected=s.state==='connected';
if(!loaded){if(!el('ssid').value&&s.ssid)picker.setSSID(s.ssid);el('hostname').value=s.nextHostname||s.hostname||'watchsync';previewHostname();}
loaded=true;
message(s.message,connected?'ok':s.state==='error'?'error':'');
controls();
if(connected){var address='http://'+(s.nextHostname||s.hostname||'watchsync')+'.local';el('hostnameDestination').href=address;el('hostnameDestination').textContent=address;el('psk').value='';el('destinations').hidden=false;el('destination').textContent='http://'+s.ip;el('destination').href='http://'+s.ip;}
if(!pickerLoaded&&token&&!connected&&s.state!=='connecting'){pickerLoaded=true;picker.load(true);}
}
function refresh(){
clearTimeout(timer);if(document.hidden||busy||loading)return;
if(picker&&picker.isBusy()){timer=setTimeout(refresh,1000);return;}
loading=true;api('/api/wifi').then(show).catch(function(error){
if(connected)message('The setup network has closed. Rejoin your normal Wi-Fi and open the address below.','ok');
else message(error.message||'Cannot reach the device. Check that you are connected to watchsync.','error');
}).finally(function(){loading=false;if(!document.hidden)timer=setTimeout(refresh,2000);});
}
function previewHostname(){el('hostnamePreview').textContent='http://'+el('hostname').value.toLowerCase()+'.local';}
el('hostname').addEventListener('input',previewHostname);
el('showPassword').addEventListener('change',function(){el('psk').type=this.checked?'text':'password';});
el('wifiForm').addEventListener('submit',function(ev){
ev.preventDefault();if(busy||!token||picker.isBusy())return;
var form=new URLSearchParams(new FormData(ev.target));form.set('token',token);
busy=true;connected=false;clearTimeout(timer);controls();el('destinations').hidden=true;message('Sending settings…');
api('/api/wifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:form.toString()}).then(show).catch(function(error){message(error.message,'error');}).finally(function(){busy=false;controls();timer=setTimeout(refresh,2000);});
});
document.addEventListener('visibilitychange',function(){clearTimeout(timer);if(!document.hidden)refresh();});
refresh();
})();
</script></body></html>
)WIFISETUP";

class WiFiConfiguration {
 public:
  static constexpr uint32_t FALLBACK_MS = 10UL * 60 * 1000;
  static constexpr uint32_t RETRY_MS = 60000;
  static constexpr uint32_t CLOSE_AP_MS = 15000;

  WiFiConfiguration(WebServer &server, watchsync::DeviceConfiguration &device) : server_(server), device_(device) {}

  void begin() {
    hostname_ = device_.hostname();
    candidateHostname_ = device_.nextHostname();
    makeToken(settingsToken_, sizeof(settingsToken_));
    loadCredentials();
    nextSSID_ = ssid_;
    nextPortal_ = ssid_.length() == 0;
    WiFi.onEvent([this](WiFiEvent_t, WiFiEventInfo_t info) {
      lastDisconnectReason_.store(info.wifi_sta_disconnected.reason);
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.persistent(false);
    WiFi.setHostname(hostname_.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(ssid_.length() > 0);
    offlineSince_ = lastReconnect_ = millis();
    if (ssid_.length()) WiFi.begin(ssid_.c_str(), psk_.c_str());
    else { WiFi.disconnect(false, false); startPortal(); }
  }

  void registerRoutes() {
    server_.on("/wifi-picker.js", HTTP_GET, [this]() {
      server_.sendHeader("Cache-Control", "no-cache");
      server_.send_P(200, "text/javascript; charset=utf-8", WIFI_PICKER_JS);
    });
    server_.on("/api/wifi/networks", HTTP_GET, [this]() {
      if (!portalClient() && !settingsClient()) { sendError(403,"Open the device page on its setup or home network."); return; }
      sendNetworks();
    });
    server_.on("/api/wifi/networks", HTTP_POST, [this]() { startScan(); });
    server_.on("/api/wifi", HTTP_GET, [this]() {
      if (!portalClient()) { sendError(403, "Connect to the watchsync setup network first."); return; }
      sendStatus();
    });
    server_.on("/api/wifi", HTTP_POST, [this]() { acceptCredentials(); });
    server_.on("/api/wifi/next", HTTP_GET, [this]() {
      if (!settingsClient()) { sendError(403, "Open the device page through your normal Wi-Fi network."); return; }
      sendNextStatus();
    });
    server_.on("/api/wifi/next", HTTP_POST, [this]() { stageCredentials(); });
    server_.on("/api/wifi/reset", HTTP_POST, [this]() { stageReset(); });
    server_.on("/api/restart", HTTP_POST, [this]() {
      if (!settingsWriteAllowed()) return;
      restartPending_ = true; restartAt_ = millis(); sendNextStatus();
    });
    const char *probes[] = {"/generate_204", "/gen_204", "/hotspot-detect.html", "/connecttest.txt", "/ncsi.txt"};
    for (const char *path : probes) server_.on(path, HTTP_GET, [this]() {
      if (!portalClient()) { server_.send(404, "text/plain", "Not found"); return; }
      server_.sendHeader("Cache-Control", "no-store");
      server_.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/");
      server_.send(302, "text/plain", "");
    });
  }

  bool portalClient() {
    return apActive_ && server_.client().localIP() == WiFi.softAPIP();
  }

  bool portalActive() const { return apActive_ || portalPreparing_; }
  uint16_t lastDisconnectReason() const { return lastDisconnectReason_.load(); }
  bool lanClient() { return settingsClient(); }
  bool localHostAllowed(String host) const {
    host.toLowerCase();
    if (host.endsWith(":80")) host.remove(host.length() - 3);
    return host == hostname_ || host == hostname_ + ".local" || host == WiFi.localIP().toString();
  }
  bool maintenanceReady() const {
    return WiFi.status() == WL_CONNECTED && !apActive_ && !candidate_ && !nextPending_ && !device_.pending() && !restartPending_ && !scanQueued_ && !scanRunning_;
  }
  const char *controlToken() const { return settingsToken_; }
  bool controlWriteAllowed() { return settingsWriteAllowed(); }
  void setStorageHooks(bool (*pause)(),void (*resume)()) { pauseStorage_=pause; resumeStorage_=resume; }

  void service() {
    const uint32_t now = millis();
    if (restartPending_ && (uint32_t)(now-restartAt_) >= 1500) ESP.restart();
    if (apActive_) dns_.processNextRequest();
    serviceScan();
    if (portalPreparing_) {
      if (!scanQueued_ && !scanRunning_) { portalPreparing_ = false; openPortal(); }
      return;
    }
    // Acknowledge the form before the shared radio changes channel.
    if (connectionQueued_) {
      if ((uint32_t)(now-submittedAt_) < 350) return;
      connectionQueued_ = false;
      WiFi.setAutoReconnect(true);
      WiFi.disconnect(false, false);
      WiFi.begin(ssid_.c_str(), psk_.c_str());
    }
    if (candidate_ && WiFi.status() != WL_CONNECTED && (uint32_t)(now-submittedAt_) >= 30000) {
      WiFi.setAutoReconnect(false);
      WiFi.disconnect(false, false);
      candidate_ = false; connectionFailed_ = true;
    }
    if (WiFi.status() == WL_CONNECTED) {
      offlineSince_ = now;
      if (!wasConnected_) {
        Serial.print("Wi-Fi connected. http://"); Serial.println(WiFi.localIP());
        wasConnected_ = true;
        linkSince_ = now;
      }
      if (candidate_) {
        if (WiFi.SSID() != ssid_ || now - linkSince_ < 1000) return;
        saveFailed_ = !saveCredentials();
        candidate_ = false;
      }
      if (apActive_ && !saveFailed_) {
        if (!closingAP_) { closingAP_ = true; connectedSince_ = now; }
        if (now - connectedSince_ >= CLOSE_AP_MS) {
          dns_.stop();
          WiFi.softAPdisconnect(true);
          WiFi.mode(WIFI_STA);
          apActive_ = closingAP_ = false;
          Serial.println("Wi-Fi setup AP closed.");
          if (device_.pending()) ESP.restart();
        }
      }
      return;
    }
    if (wasConnected_) { offlineSince_ = now; wasConnected_ = false; }
    closingAP_ = false;
    if (!apActive_ && !scanQueued_ && !scanRunning_ && ssid_.length() && now - lastReconnect_ >= RETRY_MS) {
      lastReconnect_ = now;
      WiFi.reconnect();
    }
    if (!apActive_ && now - offlineSince_ >= FALLBACK_MS &&
        (!portalAttempted_ || now - lastPortalAttempt_ >= RETRY_MS)) startPortal();
  }

  static bool validSSID(const String &s) {
    const int nullIndex = s.indexOf('\0');
    return s.length() > 0 && s.length() <= 32 && (nullIndex < 0 || (unsigned)nullIndex == s.length());
  }

  static bool validPSK(const String &s) {
    const int nullIndex = s.indexOf('\0');
    if (nullIndex >= 0 && (unsigned)nullIndex < s.length()) return false;
    if (s.length() == 0 || (s.length() >= 8 && s.length() <= 63)) return true;
    if (s.length() != 64) return false;
    for (unsigned i = 0; i < s.length(); ++i) if (!isxdigit((unsigned char)s[i])) return false;
    return true;
  }

 private:
  using StoredNetwork = watchsync::StoredWifiNetwork;
  static constexpr uint32_t NETWORK_MAGIC = watchsync::NETWORK_MAGIC;
  static constexpr uint32_t PORTAL_MAGIC = watchsync::PORTAL_MAGIC;
  WebServer &server_;
  watchsync::DeviceConfiguration &device_;
  DNSServer dns_;
  String hostname_, ssid_, psk_, nextSSID_, candidateHostname_;
  char token_[33] = {};
  char settingsToken_[33] = {};
  bool apActive_ = false, wasConnected_ = false, candidate_ = false;
  bool portalPreparing_ = false, connectionQueued_ = false, connectionFailed_ = false;
  std::atomic<uint16_t> lastDisconnectReason_{0};
  bool closingAP_ = false, saveFailed_ = false, portalAttempted_ = false;
  bool nextPortal_ = false, nextPending_ = false;
  uint32_t offlineSince_ = 0, lastReconnect_ = 0, connectedSince_ = 0;
  uint32_t lastPortalAttempt_ = 0, submittedAt_ = 0;
  uint32_t linkSince_ = 0;
  bool restartPending_ = false, scanQueued_ = false, scanRunning_ = false, scanAttempted_ = false;
  uint32_t restartAt_ = 0, scanStarted_ = 0;
  String scanResults_ = "[]";
  const char *scanError_ = nullptr;
  bool (*pauseStorage_)() = nullptr;
  void (*resumeStorage_)() = nullptr;

  bool scanWriteAllowed() {
    const char *expected = portalClient() ? token_ : settingsClient() ? settingsToken_ : nullptr;
    if (!expected || !server_.hasArg("token") || server_.arg("token") != expected) {
      sendError(403,"Reload the device page and try again."); return false;
    }
    if (candidate_ || restartPending_) { sendError(409,"Wait for the current Wi-Fi change to finish."); return false; }
    return true;
  }

  void startScan() {
    if (!scanWriteAllowed()) return;
    if (scanQueued_ || scanRunning_ || (scanAttempted_ && (uint32_t)(millis()-scanStarted_) < 10000)) { sendNetworks(); return; }
    if (ESP.getFreeHeap()<50000 || ESP.getMaxAllocHeap()<24000) {
      sendError(503,"Not enough free memory to search. Enter the network name manually."); return;
    }
    scanAttempted_ = true; scanStarted_ = millis(); scanError_ = nullptr;
    // Deliver the HTTP acknowledgement before scanning takes the shared radio off-channel.
    scanQueued_ = true;
    sendNetworks();
  }

  void serviceScan() {
    if (scanQueued_) {
      if ((uint32_t)(millis()-scanStarted_) < 350) return;
      scanQueued_ = false;
      if (candidate_ || restartPending_) { scanError_="Wait for the Wi-Fi change to finish, then refresh networks."; return; }
      if (ESP.getFreeHeap()<50000 || ESP.getMaxAllocHeap()<24000) {
        scanError_="Not enough free memory to search. Enter the network name manually."; return;
      }
      scanStarted_ = millis();
      const int16_t result = WiFi.scanNetworks(true,false,false,120);
      scanRunning_ = result == WIFI_SCAN_RUNNING || result >= 0;
      if (!scanRunning_) scanError_="Could not start the search. Refresh or enter the network name manually.";
    }
    if (!scanRunning_) return;
    const int16_t count=WiFi.scanComplete();
    if (count==WIFI_SCAN_RUNNING && (uint32_t)(millis()-scanStarted_) < 15000) return;
    scanRunning_=false;
    if (count<0) {
      esp_wifi_scan_stop(); WiFi.scanDelete();
      scanError_="Network search timed out. Refresh or enter the network name manually."; return;
    }
    struct Network { String ssid; int32_t rssi=0; bool open=false; };
    Network networks[20];
    unsigned used=0;
    // Bound both the retained list and work even in a crowded Wi-Fi environment.
    const int limit=count<255?count:255;
    for (int i=0;i<limit;++i) {
      const String ssid=WiFi.SSID(i);
      if (!validSSID(ssid)) continue;
      const int32_t rssi=WiFi.RSSI(i);
      unsigned slot=used;
      for (unsigned n=0;n<used;++n) if (networks[n].ssid==ssid) { slot=n; break; }
      if (slot<used && networks[slot].rssi>=rssi) continue;
      if (slot==used && used==20) {
        slot=0;
        for (unsigned n=1;n<used;++n) if (networks[n].rssi<networks[slot].rssi) slot=n;
        if (networks[slot].rssi>=rssi) continue;
      }
      if (slot==used) ++used;
      networks[slot].ssid=ssid; networks[slot].rssi=rssi; networks[slot].open=WiFi.encryptionType(i)==WIFI_AUTH_OPEN;
    }
    for (unsigned i=0;i<used;++i) for (unsigned j=i+1;j<used;++j) if (networks[j].rssi>networks[i].rssi) {
      Network swap=networks[i]; networks[i]=networks[j]; networks[j]=swap;
    }
    String result; result.reserve(2300); result='[';
    for (unsigned i=0;i<used;++i) {
      if (i) result+=',';
      result+="{\"ssid\":"; result+=quoteJSON(networks[i].ssid);
      result+=",\"rssi\":"; result+=networks[i].rssi;
      result+=",\"open\":"; result+=networks[i].open?"true":"false"; result+='}';
    }
    result+=']'; scanResults_=result;
    WiFi.scanDelete();
  }

  void sendNetworks() {
    String response; response.reserve(scanResults_.length()+180);
    response="{\"state\":\""; response+=(scanQueued_||scanRunning_)?"scanning":scanError_?"error":"ready";
    response+="\",\"networks\":"; response+=scanResults_;
    if (scanError_) { response+=",\"message\":"; response+=quoteJSON(scanError_); }
    response+='}'; server_.sendHeader("Cache-Control","no-store");
    server_.send(200,"application/json; charset=utf-8",response);
  }

  bool loadCredentials() {
    bool found = false;
    const bool loaded = loadStoredCredentials("watchsyncwifi", found);
    if (found) return loaded;
    // Keep upgrades connected until credentials are saved in the new namespace.
    // This literal is a compatibility key, not a setup-network name.
    return loadStoredCredentials("clocksyncwifi", found);
  }

  bool loadStoredCredentials(const char *namespaceName, bool &found) {
    found = false;
    Preferences storage;
    if (!storage.begin(namespaceName, true)) return false;
    found = storage.isKey("network");
    StoredNetwork n = {};
    const bool loaded = storage.getBytesLength("network") == sizeof(n) &&
                        storage.getBytes("network", &n, sizeof(n)) == sizeof(n);
    storage.end();
    if (!loaded) return false;
    // An explicit reset must suppress compiled secrets on every subsequent boot.
    if (n.magic == PORTAL_MAGIC) { ssid_ = ""; psk_ = ""; return true; }
    if (n.magic != NETWORK_MAGIC || n.ssid[32] || n.psk[64] ||
        !validSSID(String(n.ssid)) || !validPSK(String(n.psk))) return false;
    ssid_ = n.ssid; psk_ = n.psk;
    return true;
  }

  bool saveCredentials() {
    if (!storeCredentials(ssid_, psk_)) return false;
    if (pauseStorage_ && !pauseStorage_()) return false;
    const bool identitySaved = device_.saveHostname(candidateHostname_.c_str(), candidateHostname_.length());
    if (resumeStorage_) resumeStorage_();
    if (!identitySaved) return false;
    nextSSID_ = ssid_; nextPortal_ = nextPending_ = false;
    return true;
  }

  bool storeCredentials(const String &ssid, const String &psk, bool portalOnly = false) {
    StoredNetwork n = {};
    n.magic = portalOnly ? PORTAL_MAGIC : NETWORK_MAGIC;
    if (!portalOnly) {
      ssid.toCharArray(n.ssid, sizeof(n.ssid));
      psk.toCharArray(n.psk, sizeof(n.psk));
    }
    // Keep boot mode and credentials in the same record, including explicit resets.
    if (pauseStorage_ && !pauseStorage_()) return false;
    Preferences storage;
    if (!storage.begin("watchsyncwifi", false)) { if(resumeStorage_) resumeStorage_(); return false; }
    const bool saved = storage.putBytes("network", &n, sizeof(n)) == sizeof(n);
    storage.end();
    if(resumeStorage_) resumeStorage_();
    return saved;
  }

  static void makeToken(char *token, size_t size) {
    snprintf(token, size, "%08lx%08lx%08lx%08lx",
             (unsigned long)esp_random(), (unsigned long)esp_random(),
             (unsigned long)esp_random(), (unsigned long)esp_random());
  }

  bool settingsClient() {
    return WiFi.status() == WL_CONNECTED && server_.client().localIP() == WiFi.localIP();
  }

  bool settingsWriteAllowed() {
    if (!settingsClient()) { sendError(403, "Open the device page through your normal Wi-Fi network."); return false; }
    if (!server_.hasArg("token") || server_.arg("token") != settingsToken_) {
      sendError(403, "Reload the page and try again."); return false;
    }
    // Provisioning must finish before another request can replace its saved record.
    if (candidate_ || restartPending_) { sendError(409, "A Wi-Fi change or restart is still in progress. Wait for it to finish first."); return false; }
    return true;
  }

  void sendNextStatus() {
    const char *message = restartPending_ ? "Restarting. Join the configured network, then open the device address shown below." : nextPending_ ?
        (nextPortal_ ? "Wi-Fi settings cleared. After the next restart, connect to the watchsync setup network. The current connection stays active." :
         "Network saved for the next startup. Restart the device when ready. These credentials have not been tested yet.") :
        "Changes apply after the next restart. The current connection stays active.";
    String response; response.reserve(700);
    response = "{\"currentSSID\":"; response += quoteJSON(ssid_);
    response += ",\"nextSSID\":"; response += quoteJSON(nextSSID_);
    response += ",\"hostname\":"; response += quoteJSON(device_.hostname());
    response += ",\"nextHostname\":"; response += quoteJSON(device_.nextHostname());
    response += ",\"forceAP\":"; response += nextPortal_ ? "true" : "false";
    response += ",\"pending\":"; response += nextPending_ ? "true" : "false";
    response += ",\"restarting\":"; response += restartPending_ ? "true" : "false";
    response += ",\"canEdit\":"; response += candidate_ ? "false" : "true";
    response += ",\"message\":"; response += quoteJSON(message);
    response += ",\"token\":"; response += quoteJSON(settingsToken_); response += '}';
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "application/json; charset=utf-8", response);
  }

  void stageCredentials() {
    if (!settingsWriteAllowed()) return;
    if (!server_.hasArg("ssid") || !server_.hasArg("psk")) { sendError(400, "SSID or password is missing."); return; }
    const String ssid = server_.arg("ssid"), psk = server_.arg("psk");
    const String restart=server_.hasArg("restart")?server_.arg("restart"):String("0");
    if (restart!="0" && restart!="1") { sendError(400,"Invalid restart option."); return; }
    if (!validSSID(ssid) || !validPSK(psk)) {
      sendError(400, "SSID must be 1-32 bytes. Password must be 8-63 bytes, 64 hexadecimal characters, or empty for an open network."); return;
    }
    if (!storeCredentials(ssid, psk)) { sendError(500, "Could not save Wi-Fi settings. Try again."); return; }
    // Keep the active connection and reconnect credentials untouched until restart.
    nextSSID_ = ssid; nextPortal_ = false; nextPending_ = true;
    if (restart=="1") { restartPending_=true; restartAt_=millis(); }
    sendNextStatus();
  }

  void stageReset() {
    if (!settingsWriteAllowed()) return;
    if (!storeCredentials(String(""), String(""), true)) { sendError(500, "Could not clear Wi-Fi settings. Try again."); return; }
    nextSSID_ = ""; nextPortal_ = nextPending_ = true;
    sendNextStatus();
  }

  void startPortal() {
    if (apActive_ || portalPreparing_) return;
    lastPortalAttempt_ = millis(); portalAttempted_ = true;
    // Scan once before advertising setup; opening the page only reads this cache.
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, false);
    portalPreparing_ = true;
    scanError_ = nullptr; scanAttempted_ = scanQueued_ = true;
    scanStarted_ = millis();
  }

  void openPortal() {
    WiFi.mode(WIFI_AP_STA);
    const IPAddress address(192, 168, 4, 1), subnet(255, 255, 255, 0);
    if (!WiFi.softAPConfig(address, address, subnet) || !WiFi.softAP("watchsync", "watchsync")) {
      Serial.println("Could not start Wi-Fi setup AP; retrying later.");
      return;
    }
    makeToken(token_, sizeof(token_));
    apActive_ = true;
    dns_.start(53, "*", address);
    Serial.println("Wi-Fi setup: SSID watchsync | password watchsync | http://192.168.4.1");
  }

  static String quoteJSON(const String &value) {
    String out = "\"";
    for (unsigned i = 0; i < value.length(); ++i) {
      const unsigned char c = value[i];
      if (c == '"' || c == '\\') { out += '\\'; out += (char)c; }
      else if (c < 32) { char escaped[7]; snprintf(escaped, sizeof(escaped), "\\u%04x", c); out += escaped; }
      else out += (char)c;
    }
    out += '"'; return out;
  }

  void sendError(int code, const char *message) {
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(code, "application/json; charset=utf-8", String("{\"message\":") + quoteJSON(message) + "}");
  }

  void sendStatus() {
    const bool connected = WiFi.status() == WL_CONNECTED;
    const char *state = "ready", *message = "Enter the network name and password.";
    if (connectionFailed_) { state = "error"; message = "Could not connect. Check the network and password, then try again. Setup is still available."; }
    else if (saveFailed_) { state = "error"; message = "Connected, but settings could not be saved. Try again."; }
    else if (connected && !candidate_) { state = "connected"; message = device_.pending() ? "Connected. Restarting within 15 seconds to apply the hostname. Rejoin your normal Wi-Fi and open the address below." : "Connected. The setup network will close within 15 seconds. Rejoin your normal Wi-Fi and open the address below."; }
    else if (candidate_) {
      state = "connecting";
      message = connected ? "Connected. Checking and saving settings…" :
          millis() - submittedAt_ < 30000 ? "Connecting to the network…" : "Not connected yet. Check the settings or try again. The setup network is still available.";
    }
    String response; response.reserve(650);
    response = "{\"state\":"; response += quoteJSON(state);
    response += ",\"message\":"; response += quoteJSON(message);
    response += ",\"ssid\":"; response += quoteJSON(ssid_);
    response += ",\"hostname\":"; response += quoteJSON(device_.hostname());
    response += ",\"nextHostname\":"; response += quoteJSON(candidate_ ? candidateHostname_ : String(device_.nextHostname()));
    response += ",\"ip\":"; response += quoteJSON(connected ? WiFi.localIP().toString() : String(""));
    response += ",\"token\":"; response += quoteJSON(token_); response += '}';
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "application/json; charset=utf-8", response);
  }

  void acceptCredentials() {
    if (!portalClient()) { sendError(403, "Connect to the watchsync setup network first."); return; }
    if (!server_.hasArg("token") || server_.arg("token") != token_) { sendError(403, "Reload the page and try again."); return; }
    if (scanQueued_ || scanRunning_) { sendError(409,"Wait for the network search to finish, then connect."); return; }
    if (!server_.hasArg("ssid") || !server_.hasArg("psk")) { sendError(400, "SSID or password is missing."); return; }
    const String ssid = server_.arg("ssid"), psk = server_.arg("psk");
    if (!validSSID(ssid) || !validPSK(psk)) { sendError(400, "SSID must be 1-32 bytes. Password must be 8-63 bytes, 64 hexadecimal characters, or empty for an open network."); return; }
    String hostname = server_.hasArg("hostname") ? server_.arg("hostname") : String(device_.nextHostname());
    hostname.toLowerCase();
    if (!watchsync::validHostname(hostname.c_str(), hostname.length())) { sendError(400, "Use 1-32 lowercase letters, digits or hyphens; start and end with a letter or digit."); return; }
    candidateHostname_ = hostname;
    ssid_ = ssid; psk_ = psk;
    candidate_ = connectionQueued_ = true;
    connectionFailed_ = saveFailed_ = closingAP_ = wasConnected_ = false;
    submittedAt_ = offlineSince_ = lastReconnect_ = millis();
    sendStatus();
  }
};
