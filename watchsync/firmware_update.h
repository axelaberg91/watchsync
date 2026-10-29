#pragma once
// SPDX-License-Identifier: CC-BY-NC-4.0
// Local firmware upload and deferred startup validation by axelaberg91.
#include <Update.h>
#include <esp_ota_ops.h>
#include <new>
#include "ota_policy.h"
#include "ota_ui.h"

// Arduino 3.3.10 otherwise confirms a new image before setup() has run.
extern "C" bool verifyRollbackLater() { return true; }

class FirmwareUpdate {
 public:
  FirmwareUpdate(WebServer &server, WiFiConfiguration &wifi) : server_(server), wifi_(wifi) {}

  void begin(bool (*pause)(), void (*resume)()) {
    pause_ = pause; resume_ = resume;
    startedAt_ = millis();
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    pendingBoot_ = esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY;
    const char *headers[] = {"Origin", "Content-Type", "X-Watchsync-Update"};
    server_.collectHeaders(headers, sizeof(headers) / sizeof(headers[0]));
    server_.on("/update", HTTP_GET, [this]() {
      if (!wifi_.lanClient()) { reply(403, "Open this page through your normal Wi-Fi network."); return; }
      server_.sendHeader("Cache-Control", "no-store");
      server_.sendHeader("X-Frame-Options", "DENY");
      server_.send_P(200, "text/html; charset=utf-8", OTA_UI);
    });
    server_.on("/api/firmware", HTTP_GET, [this]() { status(); });
    server_.on("/api/firmware/prepare", HTTP_POST, [this]() { prepare(); });
    auto *handler = new (std::nothrow) UploadHandler(*this);
    routesReady_ = handler != nullptr;
    if (handler) server_.addHandler(handler);
  }

  bool restarting() const { return rebootPending_; }
  bool deviceChangesAllowed() const { return !pendingBoot_ && !bootFault_ && !receiving_ && !rebootPending_; }

  void service(bool healthy) {
    const uint32_t now = millis();
    if (rebootPending_) { if (watchsync::otaElapsed(now, rebootAt_, 1500)) ESP.restart(); return; }
    if (receiving_ && watchsync::otaElapsed(now, transferAt_, watchsync::OTA_TRANSFER_MS))
      fail(408, "Update timed out. The current firmware is still running.");
    if (ticket_[0] && watchsync::otaElapsed(now, ticketAt_, watchsync::OTA_TICKET_MS)) ticket_[0] = 0;
    if (!pendingBoot_ || bootFault_) return;
    ++bootLoops_;
    if (healthy && routesReady_ && bootLoops_ >= 100 && watchsync::otaElapsed(now, startedAt_, 10000) && pause_ && pause_()) {
      const esp_err_t result = esp_ota_mark_app_valid_cancel_rollback();
      if (resume_) resume_();
      if (result == ESP_OK) {
        pendingBoot_ = false;
        Serial.println("Firmware startup checks passed; update confirmed.");
        return;
      }
    }
    if (watchsync::otaElapsed(now, startedAt_, 60000)) {
      Serial.println("Firmware startup checks failed; requesting rollback.");
      if (pause_) pause_();
      bootFault_ = true;
      // The bootloader also rolls back an unconfirmed image after a reset.
      if (esp_ota_check_rollback_is_possible()) esp_ota_mark_app_invalid_rollback_and_reboot();
      // With no previous image, leave the failed startup unconfirmed and stopped.
      lastMessage_ = "Startup checks failed. Restore the firmware over USB.";
    }
  }

 private:
  class UploadHandler : public RequestHandler {
   public:
    explicit UploadHandler(FirmwareUpdate &owner) : owner_(owner) {}
    bool canHandle(WebServer &, HTTPMethod method, const String &uri) override {
      return method == HTTP_POST && uri == "/api/firmware/upload";
    }
    bool canRaw(WebServer &server, const String &uri) override { return canHandle(server, server.method(), uri); }
    bool handle(WebServer &, HTTPMethod, const String &) override { owner_.finish(); return true; }
    void raw(WebServer &, const String &, HTTPRaw &data) override { owner_.receive(data); }
   private:
    FirmwareUpdate &owner_;
  };

  WebServer &server_;
  WiFiConfiguration &wifi_;
  bool (*pause_)() = nullptr;
  void (*resume_)() = nullptr;
  bool routesReady_ = false, pendingBoot_ = false, bootFault_ = false;
  bool receiving_ = false, held_ = false, headerWritten_ = false, rawSeen_ = false, bodyComplete_ = false;
  bool rebootPending_ = false;
  uint32_t startedAt_ = 0, bootLoops_ = 0, ticketAt_ = 0, transferAt_ = 0, rebootAt_ = 0;
  char ticket_[33] = {};
  size_t expected_ = 0;
  int requestError_ = 0;
  const char *lastMessage_ = "Choose a watchsync firmware file to update this device.";
  const char *requestMessage_ = nullptr;
  watchsync::FirmwareImageGuard image_;

  size_t capacity() const {
    const esp_partition_t *next = esp_ota_get_next_update_partition(nullptr);
    return next && next != esp_ota_get_running_partition() ? next->size : 0;
  }

  bool sameOrigin() {
    return wifi_.localHostAllowed(server_.hostHeader()) && server_.hasHeader("Origin") &&
        server_.header("Origin") == String("http://") + server_.hostHeader();
  }

  void reply(int code, const char *message) {
    // All messages are fixed English literals, without JSON control characters.
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(code, "application/json; charset=utf-8", String("{\"message\":\"") + message + "\"}");
  }

  void status() {
    if (!wifi_.lanClient()) { reply(403, "Open this page through your normal Wi-Fi network."); return; }
    const size_t available = capacity();
    const bool ready = routesReady_ && available && !pendingBoot_ && !bootFault_ && !rebootPending_ && !receiving_ && wifi_.maintenanceReady();
    const char *state = rebootPending_ ? "restarting" : bootFault_ ? "error" : pendingBoot_ ? "confirming" : receiving_ ? "receiving" : "ready";
    const char *message = !available ? "OTA needs two app slots. Install the OTA build over USB with the Minimal SPIFFS partition scheme first." :
        bootFault_ ? lastMessage_ : pendingBoot_ ? "Checking firmware startup. This normally takes about 10 seconds." : !wifi_.maintenanceReady() ?
        "Finish Wi-Fi setup and network searches, or restart to apply pending Wi-Fi or hostname settings before updating." : lastMessage_;
    String json; json.reserve(600);
    json = "{\"version\":\"" VERSION "\",\"ready\":"; json += ready ? "true" : "false";
    json += ",\"confirmed\":"; json += !pendingBoot_ && !bootFault_ ? "true" : "false";
    json += ",\"maxSize\":"; json += (unsigned long)available;
    json += ",\"state\":\""; json += state;
    json += "\",\"bootId\":\""; json += wifi_.controlToken();
    json += "\",\"token\":\""; json += wifi_.controlToken();
    json += "\",\"message\":\""; json += message; json += "\"}";
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "application/json; charset=utf-8", json);
  }

  void prepare() {
    if (!wifi_.controlWriteAllowed()) return;
    if (!sameOrigin()) { reply(403, "Open the update page on this device and try again."); return; }
    if (pendingBoot_ || bootFault_ || receiving_ || rebootPending_ || !routesReady_ || !wifi_.maintenanceReady()) {
      reply(409, "Wait for startup checks, Wi-Fi setup and network searches to finish."); return;
    }
    const String text = server_.arg("size");
    if (!text.length() || text.length() > 8) { reply(400, "Invalid firmware file size."); return; }
    for (unsigned i = 0; i < text.length(); ++i) if (text[i] < '0' || text[i] > '9') { reply(400, "Invalid firmware file size."); return; }
    const size_t size = strtoul(text.c_str(), nullptr, 10), available = capacity();
    if (!available) { reply(409, "Install the OTA build over USB with the Minimal SPIFFS partition scheme first."); return; }
    if (size < watchsync::OTA_MIN_IMAGE_SIZE || size > available) { reply(413, "This firmware file does not fit the update slot."); return; }
    if (ESP.getFreeHeap() < 50000 || ESP.getMaxAllocHeap() < 16000) { reply(503, "Not enough free memory. Restart the device and try again."); return; }
    expected_ = size; ticketAt_ = millis();
    snprintf(ticket_, sizeof(ticket_), "%08lx%08lx%08lx%08lx", (unsigned long)esp_random(),
        (unsigned long)esp_random(), (unsigned long)esp_random(), (unsigned long)esp_random());
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "application/json; charset=utf-8", String("{\"ticket\":\"") + ticket_ + "\"}");
  }

  void release() { if (held_ && resume_) resume_(); held_ = false; }

  void fail(int code, const char *message) {
    if (Update.isRunning()) Update.abort();
    receiving_ = bodyComplete_ = false;
    requestError_ = code; requestMessage_ = lastMessage_ = message;
    release();
  }

  void reject(int code, const char *message) {
    requestError_ = code; requestMessage_ = message;
    reply(code, message);
    // Stop before reading an unauthorised or unbounded request body.
    server_.client().stop();
  }

  void receive(HTTPRaw &data) {
    if (data.status == RAW_START) {
      rawSeen_ = true; requestError_ = 0; requestMessage_ = nullptr; bodyComplete_ = false;
      if (rebootPending_ || receiving_ || pendingBoot_ || bootFault_) { reject(409, "The device is not ready for another update."); return; }
      if (!wifi_.lanClient() || !sameOrigin() || !ticket_[0] || server_.header("X-Watchsync-Update") != ticket_ ||
          watchsync::otaElapsed(millis(), ticketAt_, watchsync::OTA_TICKET_MS)) {
        reject(403, "The update request expired. Choose the file again and retry."); return;
      }
      ticket_[0] = 0;  // A ticket can be used only once.
      if (!wifi_.maintenanceReady() || server_.header("Content-Type") != "application/octet-stream" ||
          server_.clientContentLength() < 0 || size_t(server_.clientContentLength()) != expected_ || expected_ > capacity()) {
        reject(409, "The file size or device state changed. Reload this page and retry."); return;
      }
      if (!pause_ || !pause_()) { reject(503, "Could not safely pause the transmitter. Restart the device before retrying."); return; }
      held_ = true;
      if (!Update.begin(expected_, U_FLASH)) { fail(500, "Could not open the inactive firmware slot. The current firmware is still running."); reject(requestError_, requestMessage_); return; }
      image_ = watchsync::FirmwareImageGuard(); headerWritten_ = false;
      transferAt_ = millis(); receiving_ = true;
      return;
    }
    if (data.status == RAW_ABORTED) {
      if (receiving_) fail(400, "Upload interrupted. The current firmware is still running.");
      rawSeen_ = false;
      return;
    }
    if (!receiving_) return;
    if (watchsync::otaElapsed(millis(), transferAt_, watchsync::OTA_TRANSFER_MS)) {
      fail(408, "Update timed out. The current firmware is still running."); server_.client().stop(); return;
    }
    if (data.status == RAW_WRITE) {
      const size_t before = image_.received;
      if (!image_.feed(data.buf, data.currentSize, expected_)) { fail(400, "Invalid ESP32 application image. Choose watchsync.bin for Atom Lite."); reject(requestError_, requestMessage_); return; }
      if (!image_.headerReady()) return;
      size_t skip = 0;
      if (!headerWritten_) {
        skip = sizeof(image_.prefix) - before;
        if (Update.write(image_.prefix, sizeof(image_.prefix)) != sizeof(image_.prefix)) { fail(500, "Flash write failed. The current firmware is still running."); reject(requestError_, requestMessage_); return; }
        headerWritten_ = true;
      }
      const size_t count = data.currentSize - skip;
      if (count && Update.write(data.buf + skip, count) != count) { fail(500, "Flash write failed. The current firmware is still running."); reject(requestError_, requestMessage_); }
    } else if (data.status == RAW_END) {
      if (!image_.complete(expected_)) { fail(400, "Incomplete file or wrong firmware target. Choose watchsync.bin for Atom Lite GPIO25."); return; }
      bodyComplete_ = true;
    }
  }

  void finish() {
    if (!rawSeen_) { reply(415, "Send the application binary from the device update page."); return; }
    rawSeen_ = false;
    if (requestError_) { reply(requestError_, requestMessage_); return; }
    if (!receiving_ || !bodyComplete_ || !Update.end()) {
      fail(400, "Firmware verification failed. The current firmware is still running."); reply(requestError_, requestMessage_); return;
    }
    receiving_ = false; rebootPending_ = true; rebootAt_ = millis();
    lastMessage_ = "Firmware verified. Restarting the device.";
    // Keep the radio paused until reboot; NVS and the partition table are untouched.
    reply(200, lastMessage_);
  }
};
