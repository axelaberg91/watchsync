// SPDX-License-Identifier: CC-BY-NC-4.0
// OTA transaction and failure-path simulations by axelaberg91.
#define VERSION "test"
#include "ota_fakes/runtime.h"
#include "../watchsync/firmware_update.h"
#include <assert.h>
#include <algorithm>

static int pauses=0,resumes=0;
static bool pauseAllowed=true;
static bool pauseRadio(){++pauses;return pauseAllowed;}
static void resumeRadio(){++resumes;}
static std::vector<uint8_t> fixture(){
  std::vector<uint8_t> bytes(5000,0x55);
  bytes[0]=0xe9;bytes[1]=4;bytes[12]=bytes[13]=0;bytes[23]=1;
  bytes[32]=0x32;bytes[33]=0x54;bytes[34]=0xcd;bytes[35]=0xab;
  memcpy(bytes.data()+1425,watchsync::OTA_TARGET,sizeof(watchsync::OTA_TARGET));return bytes;
}
struct Harness{
  WebServer server;WiFiConfiguration wifi{server};FirmwareUpdate update{server,wifi};
  explicit Harness(bool pending=false){
    fakeNow=0;pauses=resumes=0;pauseAllowed=true;ESP=FakeESP();Update=FakeUpdate();
    fakeHasSlot=true;fakePending=pending;fakeCanRollback=true;fakeConfirmCount=fakeRollbackCount=0;fakeConfirmResult=ESP_OK;
    update.begin(pauseRadio,resumeRadio);
    server.requestHeaders["Origin"]="http://watchsync.local";
    server.requestHeaders["Content-Type"]="application/octet-stream";
    server.arguments["size"]="5000";
  }
  void prepare(){
    server.request("/api/firmware/prepare",HTTP_POST);
    if(server.responseCode==200){const size_t start=server.response.find(":\"")+2;server.requestHeaders["X-Watchsync-Update"]=server.response.substr(start,32);}
  }
  void raw(HTTPRawStatus status,const uint8_t *bytes=nullptr,size_t count=0){
    HTTPRaw data;data.status=status;data.currentSize=count;if(count)memcpy(data.buf,bytes,count);
    server.upload->raw(server,"/api/firmware/upload",data);
  }
  void body(const std::vector<uint8_t>&bytes,size_t chunk=1436){for(size_t i=0;i<bytes.size();i+=chunk)raw(RAW_WRITE,bytes.data()+i,std::min(chunk,bytes.size()-i));}
  void finish(){raw(RAW_END);server.upload->handle(server,HTTP_POST,"/api/firmware/upload");}
};
int main(){
  const auto good=fixture();
  {Harness h;h.prepare();assert(h.server.responseCode==200&&Update.begins==0);}
  {Harness h;h.server.requestHeaders["Origin"]="http://elsewhere.invalid";h.prepare();assert(h.server.responseCode==403&&Update.begins==0);}
  {Harness h;h.server.host="unrelated.invalid";h.server.requestHeaders["Origin"]="http://unrelated.invalid";h.prepare();assert(h.server.responseCode==403&&Update.begins==0);}
  {Harness h;h.wifi.lan=false;h.prepare();assert(h.server.responseCode==403&&Update.begins==0);}
  {Harness h;h.wifi.tokenValid=false;h.prepare();assert(h.server.responseCode==403);}
  {Harness h;fakeHasSlot=false;h.prepare();assert(h.server.responseCode==409&&Update.begins==0);}
  {Harness h;h.wifi.ready=false;h.prepare();assert(h.server.responseCode==409);}
  {Harness h;assert(h.update.deviceChangesAllowed());h.prepare();h.raw(RAW_START);assert(!h.update.deviceChangesAllowed());h.raw(RAW_ABORTED);assert(h.update.deviceChangesAllowed());}
  {Harness h(true);assert(!h.update.deviceChangesAllowed());for(int i=0;i<100;++i)h.update.service(true);fakeNow=10000;h.update.service(true);assert(h.update.deviceChangesAllowed());}
  {Harness h;ESP.heap=20000;h.prepare();assert(h.server.responseCode==503);}
  {Harness h;h.server.arguments["size"]="1966081";h.prepare();assert(h.server.responseCode==413);}
  {Harness h;h.prepare();fakeNow=30000;h.raw(RAW_START);assert(h.server.responseCode==403&&Update.begins==0&&pauses==0);}
  {Harness h;h.prepare();h.server.length=4999;h.raw(RAW_START);assert(h.server.responseCode==409&&Update.begins==0);}
  {Harness h;h.prepare();pauseAllowed=false;h.raw(RAW_START);assert(h.server.responseCode==503&&Update.begins==0);}
  {Harness h;h.prepare();Update.beginResult=false;h.raw(RAW_START);assert(h.server.responseCode==500&&resumes==1);}
  {Harness h;h.prepare();h.raw(RAW_START);h.raw(RAW_WRITE,good.data(),100);h.raw(RAW_ABORTED);assert(Update.aborts==1&&Update.ends==0&&resumes==1&&!h.update.restarting());h.raw(RAW_START);assert(h.server.responseCode==403&&Update.begins==1);}
  {Harness h;h.prepare();h.raw(RAW_START);fakeNow=120000;h.raw(RAW_WRITE,good.data(),100);assert(Update.aborts==1&&Update.ends==0&&resumes==1&&h.server.connection.stopped);}
  {Harness h;h.prepare();h.raw(RAW_START);auto bad=good;bad[12]=9;h.raw(RAW_WRITE,bad.data(),100);assert(Update.written.empty()&&Update.aborts==1&&resumes==1);}
  {Harness h;h.prepare();h.raw(RAW_START);Update.writeResult=false;h.raw(RAW_WRITE,good.data(),100);assert(Update.aborts==1&&resumes==1&&Update.ends==0);}
  {Harness h;h.prepare();h.raw(RAW_START);auto wrong=good;wrong[1435]^=1;h.body(wrong);h.finish();assert(h.server.responseCode==400&&Update.ends==0&&resumes==1);}
  {Harness h;h.prepare();h.raw(RAW_START);h.body(good);Update.endResult=false;h.finish();assert(h.server.responseCode==400&&Update.aborts==1&&resumes==1&&!h.update.restarting());}
  for(size_t chunk:{size_t(1),size_t(35),size_t(36),size_t(1436)}){
    Harness h;h.prepare();h.raw(RAW_START);h.body(good,chunk);h.finish();
    assert(h.server.responseCode==200&&Update.ends==1&&Update.written==good&&pauses==1&&resumes==0&&h.update.restarting());
    h.raw(RAW_START);assert(resumes==0&&Update.begins==1);
    fakeNow=1500;h.update.service(true);assert(ESP.restarts==1);
  }
  {Harness h(true);h.wifi.ready=false;for(int i=0;i<100;++i)h.update.service(true);assert(fakeConfirmCount==0);fakeNow=10000;h.update.service(true);assert(fakeConfirmCount==1&&pauses==1&&resumes==1);}
  {Harness h(true);fakeNow=60000;h.update.service(false);assert(fakeRollbackCount==1&&pauses==1&&resumes==0);h.prepare();assert(h.server.responseCode==409);}
  {Harness h(true);fakeCanRollback=false;fakeNow=60000;h.update.service(false);h.server.request("/api/firmware",HTTP_GET);assert(fakeRollbackCount==0&&h.server.response.find("Startup checks failed. Restore the firmware over USB.")!=std::string::npos&&resumes==0);}
  {Harness h(true);for(int i=0;i<100;++i)h.update.service(true);pauseAllowed=false;fakeNow=60000;h.update.service(true);assert(fakeRollbackCount==1);}
  puts("PASS OTA server: password-free preparation, token, origin, LAN, capacity, memory, expiry, pause, abort, replay, write failure, boot validation and rollback");
}
