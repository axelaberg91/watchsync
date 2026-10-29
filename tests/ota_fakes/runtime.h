#pragma once
// SPDX-License-Identifier: CC-BY-NC-4.0
// Minimal host-only HTTP/runtime simulation by axelaberg91.
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <functional>
#include <map>
#include <string>
#include <vector>

class String {
 public:
  std::string value;
  String()=default;
  String(const char *s):value(s){}
  String(const std::string &s):value(s){}
  size_t length()const{return value.size();}
  const char *c_str()const{return value.c_str();}
  void reserve(size_t n){value.reserve(n);}
  char operator[](size_t n)const{return value[n];}
  String &operator+=(const char *s){value+=s;return *this;}
  String &operator+=(char c){value+=c;return *this;}
  String &operator+=(unsigned long n){value+=std::to_string(n);return *this;}
  friend String operator+(const String&a,const String&b){return a.value+b.value;}
  friend bool operator==(const String&a,const String&b){return a.value==b.value;}
  friend bool operator!=(const String&a,const String&b){return !(a==b);}
};
enum HTTPMethod{HTTP_GET,HTTP_POST};
enum HTTPRawStatus{RAW_START,RAW_WRITE,RAW_END,RAW_ABORTED};
struct HTTPRaw{HTTPRawStatus status=RAW_START;size_t currentSize=0;uint8_t buf[1436]={};};
class WebServer;
class RequestHandler{
 public:
  virtual ~RequestHandler()=default;
  virtual bool canHandle(WebServer&,HTTPMethod,const String&){return false;}
  virtual bool canRaw(WebServer&,const String&){return false;}
  virtual bool handle(WebServer&,HTTPMethod,const String&){return false;}
  virtual void raw(WebServer&,const String&,HTTPRaw&){}
};
struct FakeClient{bool stopped=false;void stop(){stopped=true;}};
class WebServer{
 public:
  std::map<std::pair<int,std::string>,std::function<void()>> routes;
  std::map<std::string,String> requestHeaders,arguments;
  RequestHandler *upload=nullptr;
  FakeClient connection;
  int responseCode=0,length=5000;
  std::string response;
  String host="watchsync.local";
  HTTPMethod currentMethod=HTTP_POST;
  ~WebServer(){delete upload;}
  void collectHeaders(const char**,size_t){}
  void on(const char *uri,HTTPMethod method,std::function<void()> fn){routes[{method,uri}]=fn;}
  void addHandler(RequestHandler *p){upload=p;}
  void sendHeader(const char*,const char*){}
  void send(int code,const char*,const String &body){responseCode=code;response=body.value;}
  void send_P(int code,const char*,const char *body){responseCode=code;response=body;}
  bool hasHeader(const char *name){return requestHeaders.count(name);}
  String header(const char *name){return requestHeaders[name];}
  String hostHeader(){return host;}
  String arg(const char *name){return arguments[name];}
  int clientContentLength(){return length;}
  FakeClient &client(){return connection;}
  HTTPMethod method(){return currentMethod;}
  void request(const char *uri,HTTPMethod method){responseCode=0;response.clear();currentMethod=method;routes.at({method,uri})();}
};
struct WiFiConfiguration{
  WebServer &server;
  bool lan=true,ready=true,tokenValid=true;
  explicit WiFiConfiguration(WebServer &s):server(s){}
  bool lanClient(){return lan;}
  bool localHostAllowed(const String &host){return host=="watchsync.local"||host=="watchsync.local:80"||host=="192.0.2.1";}
  bool maintenanceReady()const{return ready;}
  const char *controlToken()const{return "boot-token";}
  bool controlWriteAllowed(){if(tokenValid&&lan)return true;server.send(403,"application/json","{}");return false;}
};
inline uint32_t fakeNow=0;
inline uint32_t millis(){return fakeNow;}
inline uint32_t esp_random(){static uint32_t counter=100;return ++counter;}
struct FakeESP{
  int restarts=0;size_t heap=100000,block=50000;
  size_t getFreeHeap(){return heap;}
  size_t getMaxAllocHeap(){return block;}
  void restart(){++restarts;}
};
inline FakeESP ESP;
struct FakeSerial{void println(const char*){}};
inline FakeSerial Serial;
