#ifndef HTTP_DRIVER_H
#define HTTP_DRIVER_H

#include "esp_err.h"

typedef esp_err_t (*http_data_callback_t)(uint8_t *data, size_t len);
esp_err_t http_download_stream(const char *url, http_data_callback_t callback);
esp_err_t http_get_etag(const char *url, char *etag_out);

#endif