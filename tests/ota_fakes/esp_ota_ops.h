#pragma once
// SPDX-License-Identifier: CC-BY-NC-4.0
// Host-only boot partition simulation by axelaberg91.
using esp_err_t=int;
static constexpr esp_err_t ESP_OK=0;
struct esp_partition_t { size_t size; };
enum esp_ota_img_states_t { ESP_OTA_IMG_VALID, ESP_OTA_IMG_PENDING_VERIFY };
inline esp_partition_t fakeCurrent{0x1e0000},fakeNext{0x1e0000};
inline bool fakeHasSlot=true,fakePending=false,fakeCanRollback=true;
inline int fakeConfirmResult=ESP_OK,fakeConfirmCount=0,fakeRollbackCount=0;
inline const esp_partition_t *esp_ota_get_running_partition(){return &fakeCurrent;}
inline const esp_partition_t *esp_ota_get_next_update_partition(void*){return fakeHasSlot?&fakeNext:&fakeCurrent;}
inline esp_err_t esp_ota_get_state_partition(const esp_partition_t*,esp_ota_img_states_t *state){*state=fakePending?ESP_OTA_IMG_PENDING_VERIFY:ESP_OTA_IMG_VALID;return ESP_OK;}
inline esp_err_t esp_ota_mark_app_valid_cancel_rollback(){++fakeConfirmCount;return fakeConfirmResult;}
inline bool esp_ota_check_rollback_is_possible(){return fakeCanRollback;}
inline esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot(){++fakeRollbackCount;return ESP_OK;}
