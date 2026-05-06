#ifndef ESP_BOOTLOADER_H
#define ESP_BOOTLOADER_H

#include <stdint.h>
#include "esp_err.h"

#define STM32_APP_MAX_SIZE (56 * 1024)
#define UART_PORT_NUM      UART_NUM_2

#define BL_ADD_BASE            0x08000000
#define APP_ADD_BASE           0x08002000
#define BL_UPDATE_KEY_1        0xCC
#define BL_UPDATE_KEY_2        0xDD

typedef enum{
	BL_START         = 0x00,
	BL_PROGRAM		 = 0x01,
	BL_ERASE         = 0x02,
	BL_JUMP          = 0x03
}BL_CMD_t;

typedef enum{
	BL_ACK           = 0x4,
	BL_NACK          = 0x5,
	BL_OVERLOAD      = 0x6,
	BL_NONE          = 0x7
}BL_ACK_NACK_t;

extern uint8_t bl_buffer[256];
extern volatile BL_CMD_t bl_cmd;
extern volatile uint8_t bl_length;  //of data
extern uint8_t bl_data[256];
extern uint8_t button_status;

uint16_t BL_CRC16();
esp_err_t BL_Wait_ACK(uint32_t timeout_ms);
esp_err_t BL_Handshake();
esp_err_t BL_Send_Start();
esp_err_t BL_Send_Erase_App(uint8_t pages);
esp_err_t BL_Send_Program(uint8_t *data, uint8_t len);
esp_err_t BL_Send_Jump();

#endif