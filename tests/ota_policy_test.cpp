// SPDX-License-Identifier: CC-BY-NC-4.0
// Firmware image and deadline regression checks by axelaberg91.
#include "../watchsync/ota_policy.h"
#include <assert.h>
#include <stdio.h>
#include <vector>

static std::vector<uint8_t> fixture() {
  std::vector<uint8_t> bytes(5000, 0x55);
  bytes[0]=0xe9; bytes[1]=4; bytes[12]=bytes[13]=0; bytes[23]=1;
  bytes[32]=0x32; bytes[33]=0x54; bytes[34]=0xcd; bytes[35]=0xab;
  memcpy(bytes.data()+1425,watchsync::OTA_TARGET,sizeof(watchsync::OTA_TARGET));
  return bytes;
}

int main() {
  const auto good=fixture();
  for (size_t chunk=1; chunk<=1500; ++chunk) {
    watchsync::FirmwareImageGuard image;
    for(size_t pos=0; pos<good.size(); pos+=chunk) {
      const size_t count=chunk<good.size()-pos?chunk:good.size()-pos;
      assert(image.feed(good.data()+pos,count,good.size()));
    }
    assert(image.complete(good.size()));
  }
  for (size_t offset: {size_t(0),size_t(1),size_t(12),size_t(13),size_t(23),size_t(32),size_t(35)}) {
    auto bad=good; bad[offset]=0xff;
    watchsync::FirmwareImageGuard image;
    assert(!image.feed(bad.data(),bad.size(),bad.size()));
    assert(!image.complete(bad.size()));
  }
  for(size_t length=0; length<good.size(); ++length) {
    watchsync::FirmwareImageGuard image;
    assert(image.feed(good.data(),length,good.size()));
    assert(!image.complete(good.size()));
  }
  auto wrong=good; wrong[1435]^=1;
  watchsync::FirmwareImageGuard target;
  assert(target.feed(wrong.data(),wrong.size(),wrong.size()));
  assert(!target.complete(wrong.size()));
  watchsync::FirmwareImageGuard overflow;
  assert(!overflow.feed(good.data(),good.size(),good.size()-1));
  assert(overflow.received==0);
  assert(!watchsync::otaElapsed(0xfffffff0U,0xffffffe0U,32));
  assert(watchsync::otaElapsed(0x10U,0xffffffe0U,32));
  assert(!watchsync::otaElapsed(29999,0,watchsync::OTA_TICKET_MS));
  assert(watchsync::otaElapsed(30000,0,watchsync::OTA_TICKET_MS));
  puts("PASS OTA policy: chunk boundaries, target and chip checks, truncation, overflow, monotonic deadlines");
}
