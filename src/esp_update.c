#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_https_ota.h"
#include <cJSON.h>
#include <semver.h>

static const char *TAG = "ota";

esp_err_t get_device_id(char* device_id, size_t device_id_size) {
	uint8_t mac_address[6];
	const esp_err_t err = esp_base_mac_addr_get(mac_address);
	if (err != ESP_OK) {
		return err;
	}
	snprintf(device_id, device_id_size, "%02x%02x%02x%02x%02x%02x", mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5]);
	return err;
}

void esp_update(char update_server[], char application[], char current_version[]) {
	char device_id[12];
	esp_err_t err = get_device_id(device_id, sizeof(device_id));
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to get device ID: %s", esp_err_to_name(err));
		return;
	}

	ESP_LOGI(TAG, "Starting update... This is version %s with device id %s.", current_version, device_id);

	char metadata_url[256];
	snprintf(metadata_url, sizeof(metadata_url), "%s/%s/latest.json?current_version=%s&device_id=%s", update_server, application, current_version, device_id);

	ESP_LOGI(TAG, "Fetching latest version from %s...", metadata_url);
	esp_http_client_config_t metadata_http_config = {
		.url = metadata_url,
	};
	esp_http_client_handle_t client = esp_http_client_init(&metadata_http_config);
	err = esp_http_client_open(client, 0);

	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to perform HTTP request: %s", esp_err_to_name(err));
		return;
	}

	const int content_length = esp_http_client_fetch_headers(client);
	if (content_length <= 0) {
		ESP_LOGE(TAG, "HTTP request failed. Content length was %d.", content_length);
		return;
	}

	const int status_code = esp_http_client_get_status_code(client);
	if (status_code < 200 || status_code > 299) {
		ESP_LOGE(TAG, "HTTP request failed with status code %d.", status_code);
		return;
	}
	ESP_LOGI(TAG, "HTTP request succeeded with status code %d and content length %d.", status_code, content_length);

	char response[content_length + 1];
	int bytes_read = esp_http_client_read_response(client, response, content_length);
	if (bytes_read != content_length) {
		ESP_LOGE(TAG, "Expected to read %d bytes, but actually read %d.", content_length, bytes_read);
		return;
	}
	response[bytes_read] = 0;
	ESP_LOGI(TAG, "Read response was:\n%s", response);

	if (esp_http_client_is_complete_data_received(client)) {
		ESP_LOGI(TAG, "Response read completely without errors.");
	}
	else {
		ESP_LOGE(TAG, "Response was not read or had errors.");
		return;
	}

	const cJSON* parsed = cJSON_Parse(response);
	const char* update_version = cJSON_GetObjectItemCaseSensitive(parsed, "version")->valuestring;
	const char* update_url = cJSON_GetObjectItemCaseSensitive(parsed, "url")->valuestring;

	semver_t parsed_current_version = {};
	semver_t parsed_update_version = {};
	semver_parse(current_version, &parsed_current_version);
	semver_parse(update_version, &parsed_update_version);

	ESP_LOGI(TAG, "Latest version is %s with URL %s.", update_version, update_url);

	if (semver_compare(parsed_current_version, parsed_update_version) >= 0) {
		ESP_LOGI(TAG, "Already up to date.");
		return;
	}

	ESP_LOGI(TAG, "Not up to date. Updating...");

	char cert[0] = {};
	esp_http_client_config_t update_http_config = {
		.url = update_url,
		.transport_type = HTTP_TRANSPORT_OVER_TCP,
		.cert_pem = cert,
	};
	err = esp_https_ota(&update_http_config);
	if (err == ESP_OK) {
		ESP_LOGE(TAG, "Firmware update finished.");
		esp_restart();
	}
	else {
		ESP_LOGE(TAG, "Firmware update failed with error %d.", err);
	}
}
