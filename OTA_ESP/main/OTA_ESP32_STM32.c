#include <stdio.h>
#include <string.h> 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_random.h"
#include "esp_log.h"
#include "WIFI.h"
#include "HTTP.h"
#include "ESP_BOOTLOADER.h"

static const char *TAG = "OTA_MAIN";

#define TXD_PIN          GPIO_NUM_17
#define RXD_PIN          GPIO_NUM_16
#define UART_PORT_NUM    UART_NUM_2
#define BUTTON_PIN       GPIO_NUM_0
#define LED_NEW_VERSION  GPIO_NUM_4
#define LED_LOADING      GPIO_NUM_2

static uint8_t ota_buffer[128];
static size_t ota_buf_idx = 0;
static char current_server_etag[64] = "";
static char last_applied_etag[64] = "";
static bool is_new_version_available = false;

void init_uart_bootloader() {
    uart_config_t cfg = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_PORT_NUM, &cfg);
    uart_set_pin(UART_PORT_NUM, TXD_PIN, RXD_PIN, -1, -1);
    uart_driver_install(UART_PORT_NUM, 1024, 0, 0, NULL, 0);

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_conf);
}
void init_gpio_leds() {
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_NEW_VERSION) | (1ULL << LED_LOADING),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&led_conf);
    
    gpio_set_level(LED_NEW_VERSION, 0);
    gpio_set_level(LED_LOADING, 0);
}

esp_err_t my_ota_data_cb(uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        ota_buffer[ota_buf_idx++] = data[i];

        if (ota_buf_idx == 128) {
            if (BL_Send_Program(ota_buffer, 128) != ESP_OK) {
                return ESP_FAIL;
            }
            ota_buf_idx = 0;
        }
    }
    ESP_LOGI(TAG, "Received %d bytes from stream...", (int)len);
    return ESP_OK; 
}

esp_err_t OTA_Finalize_Padding() {
    if (ota_buf_idx > 0) {
        memset(&ota_buffer[ota_buf_idx], 0xFF, 128 - ota_buf_idx);
        if (BL_Send_Program(ota_buffer, 128) != ESP_OK) {
            return ESP_FAIL;
        }
        ota_buf_idx = 0;
    }
    return ESP_OK;
}

void run_ota_process() {
    gpio_set_level(LED_NEW_VERSION, 0);
    gpio_set_level(LED_LOADING, 0);

    ESP_LOGW(TAG, "OTA process starting...");
    
    if (BL_Handshake() != ESP_OK) {
        ESP_LOGE(TAG, "STM32 does not respond!");
        return;
    }
    if (BL_Send_Start() != ESP_OK) {
        ESP_LOGE(TAG, "Start CMD failed!");
        return;
    }
    if (BL_Send_Erase_App(25) != ESP_OK) {
        ESP_LOGE(TAG, "Erase Flash failed!");
        return;
    }

    gpio_set_level(LED_LOADING, 1);
    ESP_LOGI(TAG, "STM32 is ready to receive Data.");

    const char *base_url = "https://raw.githubusercontent.com/vophucthinh2006/esp32-stm32-ota-system/main/BOOTLOADER_TESTING.bin";
    char url_with_cache_buster[256];
    snprintf(url_with_cache_buster, sizeof(url_with_cache_buster), "%s?v=%lu", base_url, (unsigned long)esp_random());
    
    ESP_LOGI(TAG, "Requesting URL: %s", url_with_cache_buster);

    if (http_download_stream(url_with_cache_buster, my_ota_data_cb) == ESP_OK) {
        if (OTA_Finalize_Padding() == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(100)); 
            if (BL_Send_Jump() == ESP_OK) {
                gpio_set_level(LED_LOADING, 0);
                ESP_LOGI(TAG, "OTA process successful! STM32 is starting new app.");
                strcpy(last_applied_etag, current_server_etag);
                is_new_version_available = false;
            }
        } else {
            gpio_set_level(LED_LOADING, 0);
            ESP_LOGE(TAG, "Padding process failed!");
        }
    } else {
        gpio_set_level(LED_LOADING, 0);
        ESP_LOGE(TAG, "OTA process failed!");
    }
}

void ota_control_task(void *pv) {
    const char *base_url = "https://raw.githubusercontent.com/vophucthinh2006/esp32-stm32-ota-system/main/BOOTLOADER_TESTING.bin";
    char check_url[256];
    
    while(1) {
        if (wifi_is_connected()) {
            char new_etag[64] = "";

            snprintf(check_url, sizeof(check_url), "%s?nocache=%lu", base_url, (unsigned long)esp_random());

            if (http_get_etag(check_url, new_etag) == ESP_OK) {

                if (strcmp(new_etag, last_applied_etag) != 0) {

                    if (strlen(new_etag) > 0) {
                        strcpy(current_server_etag, new_etag);
                        is_new_version_available = true;
                        gpio_set_level(LED_NEW_VERSION, 1);
                        ESP_LOGW(TAG, "NEW VERSION FOUND! ETag: %s", new_etag);
                    }
                } else {

                    gpio_set_level(LED_NEW_VERSION, 0);
                    is_new_version_available = false; 
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10000)); 
    }
}
void check_for_updates() {
    const char *base_url = "https://raw.githubusercontent.com/vophucthinh2006/esp32-stm32-ota-system/main/BOOTLOADER_TESTING.bin";
    char check_url[256];
    char new_etag[64] = "";

    if (wifi_is_connected()) {
        snprintf(check_url, sizeof(check_url), "%s?nocache=%lu", base_url, (unsigned long)esp_random());
        
        if (http_get_etag(check_url, new_etag) == ESP_OK) {
            if (strcmp(new_etag, last_applied_etag) != 0) {
                if (strlen(new_etag) > 0) {
                    strcpy(current_server_etag, new_etag);
                    is_new_version_available = true;
                    gpio_set_level(LED_NEW_VERSION, 1);
                    ESP_LOGW(TAG, "CHECK: NEW VERSION FOUND! ETag: %s", new_etag);
                }
            } else {
                gpio_set_level(LED_NEW_VERSION, 0);
                is_new_version_available = false;
                ESP_LOGI(TAG, "CHECK: Device is up to date.");
            }
        }
    }
}

void app_main(void) {

    init_uart_bootloader();
    init_gpio_leds();
    vTaskDelay(pdMS_TO_TICKS(1000));

    wifi_init_sta("thinhhh", "Thinh0602,");
    while(!wifi_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGI(TAG, "WiFi is ready.");

    xTaskCreate(ota_control_task, "ota_control_task", 4096, NULL, 5, NULL);

    uint8_t incoming;
    while(1) {

        if (gpio_get_level(BUTTON_PIN) == 0) {
            vTaskDelay(pdMS_TO_TICKS(50)); // Debounce
            if (gpio_get_level(BUTTON_PIN) == 0) {
                if (is_new_version_available) {
                    run_ota_process();
                    check_for_updates();
                } else {
                    ESP_LOGI(TAG, "No new version found.");
                }
            }
        }

        // Read log from STM32 for debugging
        if (uart_read_bytes(UART_PORT_NUM, &incoming, 1, pdMS_TO_TICKS(10)) > 0) {
            putchar(incoming);
            fflush(stdout);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1)); 
    }
}