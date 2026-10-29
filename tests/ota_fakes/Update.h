#pragma once
// SPDX-License-Identifier: CC-BY-NC-4.0
// Host-only flash writer simulation by axelaberg91.
#include <vector>
static constexpr int U_FLASH = 0;
struct FakeUpdate {
  bool running=false, beginResult=true, writeResult=true, endResult=true;
  int begins=0, ends=0, aborts=0;
  size_t expected=0;
  std::vector<uint8_t> written;
  bool isRunning() const { return running; }
  bool begin(size_t size,int) { ++begins; expected=size; running=beginResult; written.clear(); return beginResult; }
  size_t write(uint8_t *bytes,size_t count) {
    if(!writeResult)return 0;
    written.insert(written.end(),bytes,bytes+count); return count;
  }
  void abort() { running=false; ++aborts; }
  bool end() { ++ends; const bool ok=endResult&&written.size()==expected; if(ok)running=false; return ok; }
};
inline FakeUpdate Update;
