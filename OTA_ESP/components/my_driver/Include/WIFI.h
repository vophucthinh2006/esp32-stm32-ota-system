#ifndef WIFI_DRIVER_H
#define WIFI_DRIVER_H

#include <stdbool.h>
#include "esp_err.h"

esp_err_t wifi_init_sta(const char *ssid, const char *password);
bool wifi_is_connected(void);

#endif