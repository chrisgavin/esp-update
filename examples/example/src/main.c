#include "esp_update.h"

void app_main() {
	esp_update("http://firmware.example.com", "my-application", "1.0.0");
}
