#include <string.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "HTTP.h"

static const char *TAG = "HTTP_DRIVER";

typedef struct {
    http_data_callback_t data_callback;
    int64_t total_received;
    char *etag_buffer;
} http_user_data_t;

static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    http_user_data_t *user_data = (http_user_data_t *)evt->user_data;

    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "[Event] Error");
            break;
            
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "[Event] Connected to Server");
            break;

        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "[Event] Header Sent");
            break;

        case HTTP_EVENT_ON_HEADER:
            // Nếu server gửi Header ETag, lưu nó vào buffer trong user_data
            if (user_data && user_data->etag_buffer && strcasecmp(evt->header_key, "ETag") == 0) {
                strncpy(user_data->etag_buffer, evt->header_value, 63);
                user_data->etag_buffer[63] = '\0'; // Đảm bảo kết thúc chuỗi
                ESP_LOGD(TAG, "[Header] %s: %s", evt->header_key, evt->header_value);
            }
            break;

        case HTTP_EVENT_ON_DATA:
            if (evt->data_len > 0 && user_data->data_callback != NULL) {
                user_data->total_received += evt->data_len;
                ESP_LOGD(TAG, "[Data] Received: %d bytes (Total: %lld)", evt->data_len, user_data->total_received);

                if (user_data->data_callback(evt->data, evt->data_len) != ESP_OK) {
                    ESP_LOGE(TAG, "Callback failed, aborting download...");
                    return ESP_FAIL; 
                }
            }
            break;

        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "[Event] Finished");
            break;

        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "[Event] Disconnected");
            break;

        default:
            break;
    }
    return ESP_OK;
}

esp_err_t http_get_etag(const char *url, char *etag_out) {
    if (etag_out == NULL) return ESP_ERR_INVALID_ARG;
    
    http_user_data_t user_data = {
        .etag_buffer = etag_out,
        .data_callback = NULL
    };

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handler,
        .user_data = &user_data,
        .method = HTTP_METHOD_HEAD,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .skip_cert_common_name_check = true,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get ETag: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    return err;
}

esp_err_t http_download_stream(const char *url, http_data_callback_t callback) {
    http_user_data_t user_data = {
        .data_callback = callback,
        .total_received = 0,
        .etag_buffer = NULL
    };

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handler,
        .user_data = &user_data,
        .buffer_size = 4096,
        .timeout_ms = 15000,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .skip_cert_common_name_check = true,
        .keep_alive_enable = true,
        .disable_auto_redirect = false,
        .max_redirection_count = 10,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "User-Agent", "ESP32-STM32-OTA-Client");
    esp_http_client_set_header(client, "Cache-Control", "no-cache, no-store, must-revalidate");
    esp_http_client_set_header(client, "Pragma", "no-cache");
    esp_http_client_set_header(client, "Expires", "0");

    ESP_LOGI(TAG, "Starting stream from: %s", url);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        int64_t content_length = esp_http_client_get_content_length(client);
        
        if (status_code == 200 || status_code == 206) {
            ESP_LOGI(TAG, "HTTP SUCCESS | Status: %d | Size: %lld bytes", status_code, content_length);
        } else {
            ESP_LOGE(TAG, "HTTP ERROR | Status: %d", status_code);
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "HTTP Perform failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}