#ifndef __ESP_UPDATE_H
#define __ESP_UPDATE_H

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t esp_update(const char* update_server, const char* application, const char* current_version);

#ifdef __cplusplus
}
#endif
#endif
