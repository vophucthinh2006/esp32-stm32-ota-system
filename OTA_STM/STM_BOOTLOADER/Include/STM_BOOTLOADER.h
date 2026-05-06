#ifndef STM_BOOTLOADER_H
#define STM_BOOTLOADER_H
#include <stdint.h>
#include "FLASH.h"
#include "EXTI.h"
#include "STK.h"
#include "SCB.h"
#include "UART.h"
#include "TIM.h"

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

typedef enum{
	BL_WAIT_KEY_1,
	BL_WAIT_KEY_2,
	BL_WAIT_KEY_VERIFIED
}BL_WAIT_KEY_t;

typedef void (*pFunction)(void);

extern uint8_t bl_buffer[260];
extern volatile BL_CMD_t bl_cmd;
extern volatile uint8_t bl_length;  //of data
extern uint8_t bl_data[250];
extern volatile uint32_t bl_total_length;
extern volatile uint8_t bl_update_signal;
extern volatile BL_ACK_NACK_t bl_ack_state;
extern volatile BL_WAIT_KEY_t bl_wait_key;
extern volatile uint8_t start_wait_count;
extern volatile uint8_t second_wait_count;

uint8_t UART_HasData(UART_t UART_);
uint16_t BL_CRC16();
void BL_Get_Packet();
void BL_Packet_Handler();
void BL_JumpToBootloader(void);
void BL_JumpToApplication(void);
void BL_Update_Verify();

#endif
