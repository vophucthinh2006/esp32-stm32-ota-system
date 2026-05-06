#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "ESP_BOOTLOADER.h"

uint8_t bl_buffer[256];
volatile BL_CMD_t bl_cmd;
volatile uint8_t bl_length;  //of data
uint8_t bl_data[256];
uint8_t button_status = 0;

uint16_t BL_CRC16(){
	uint16_t crc = 0x0000;
	uint16_t polynomial = 0x1021; //x^16 + x^12 + x^5 + 1

    for (int i = 0; i < 2 + bl_length; i++){
        uint8_t byte;

        if (i == 0) byte = bl_cmd;
        else if (i == 1) byte = bl_length;
        else byte = bl_buffer[4 + (i - 2)];

        for (int j = 7; j >= 0; j--){
            uint8_t crc_bit = (byte >> j) & 1;
            uint8_t crc_msb  = (crc >> 15) & 1;

            crc <<= 1;

            if (crc_msb ^ crc_bit){
                crc ^= polynomial;
            }
        }
    }
    return crc;
}

esp_err_t BL_Wait_ACK(uint32_t timeout_ms) {
    uint8_t ack_signal = 0;

    int len = uart_read_bytes(UART_PORT_NUM, &ack_signal, 1, portMAX_DELAY/*pdMS_TO_TICKS(timeout_ms)*/);

    if (len > 0){
        if (ack_signal == BL_ACK) return ESP_OK;
        if (ack_signal == BL_NACK) return ESP_FAIL;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t BL_Handshake() {
    uint8_t k1 = BL_UPDATE_KEY_1, k2 = BL_UPDATE_KEY_2, res;
    uart_write_bytes(UART_PORT_NUM, (const char*)&k1, 1);
    if (uart_read_bytes(UART_PORT_NUM, &res, 1, pdMS_TO_TICKS(500)) <= 0 || res != BL_ACK) return ESP_FAIL;
    
    vTaskDelay(pdMS_TO_TICKS(50));
    
    uart_write_bytes(UART_PORT_NUM, (const char*)&k2, 1);
    if (uart_read_bytes(UART_PORT_NUM, &res, 1, pdMS_TO_TICKS(500)) <= 0 || res != BL_ACK) return ESP_FAIL;
    
    return ESP_OK;
}

esp_err_t BL_Send_Start() {
    bl_cmd = BL_START;
    bl_length = 0;
    
    uint16_t crc = BL_CRC16();
    uint8_t pkt[] = {0xAA, 0xBB, BL_START, 0, (uint8_t)(crc >> 8), (uint8_t)crc};
    uart_write_bytes(UART_PORT_NUM, (const char*)pkt, 6);
    return BL_Wait_ACK(500);
}

esp_err_t BL_Send_Erase_App(uint8_t pages) {
    bl_cmd = BL_ERASE;
    bl_length = 1;
    bl_buffer[4] = pages;
    
    uint16_t crc = BL_CRC16();
    uint8_t pkt[] = {0xAA, 0xBB, BL_ERASE, 1, pages, (uint8_t)(crc >> 8), (uint8_t)crc};
    uart_write_bytes(UART_PORT_NUM, (const char*)pkt, 7);
    return BL_Wait_ACK(2000);
}

esp_err_t BL_Send_Program(uint8_t *data, uint8_t len) {
    bl_cmd = BL_PROGRAM;
    bl_length = len;

    memcpy(&bl_buffer[4], data, len);
    
    uint16_t crc = BL_CRC16();
    uint8_t head[] = {0xAA, 0xBB, bl_cmd, bl_length};
    uart_write_bytes(UART_PORT_NUM, (const char*)head, 4);
    uart_write_bytes(UART_PORT_NUM, (const char*)data, len);
    uint8_t crc_b[] = {(uint8_t)(crc >> 8), (uint8_t)crc};
    uart_write_bytes(UART_PORT_NUM, (const char*)crc_b, 2);
    return BL_Wait_ACK(1000);
}

esp_err_t BL_Send_Jump() {
    bl_cmd = BL_JUMP;
    bl_length = 0;
    
    uint16_t crc = BL_CRC16();
    uint8_t pkt[] = {0xAA, 0xBB, BL_JUMP, 0, (uint8_t)(crc >> 8), (uint8_t)crc};
    uart_write_bytes(UART_PORT_NUM, (const char*)pkt, 6);
    return BL_Wait_ACK(500);
}