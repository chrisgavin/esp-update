#ifndef __ESP_UPDATE_H
#define __ESP_UPDATE_H

#include <esp_err.h>

esp_err_t esp_update(char update_server[], char application[], char current_version[]);

#endif
