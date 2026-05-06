#include "STM_BOOTLOADER.h"


uint8_t bl_buffer[260];
volatile BL_CMD_t bl_cmd;
volatile uint8_t bl_length;  //of data
uint8_t bl_data[250];
volatile uint32_t bl_total_length = 0;
volatile uint8_t bl_update_signal;
volatile BL_ACK_NACK_t bl_ack_state = BL_NONE;
volatile BL_WAIT_KEY_t bl_wait_key = BL_WAIT_KEY_1;
volatile uint8_t start_wait_count = 0;
volatile uint8_t second_wait_count = 0;

uint8_t UART_HasData(UART_t UART_){
	if (UART_ == UART1) return (UART1_SR & (1 << 5)) ? 1 : 0;
	if (UART_ == UART2) return (UART2_SR & (1 << 5)) ? 1 : 0;
	return (UART3_SR & (1 << 5)) ? 1 : 0;
}

void TIM2_IRQHandler(void){
	if ((TIM2_SR & (1 << 0)) && (start_wait_count == 1)){

		if (second_wait_count <= 5){
			second_wait_count ++;
		}

		TIM2_SR &= ~(1 << 0);
	}
}

void USART1_IRQHandler(void){
	if (UART1_SR & (1 << 5)){
		bl_update_signal = UART1_DR;
	}
	if (UART1_SR & (1 << 1 | 1 << 2 | 1 << 3)){
		volatile uint8_t dummy = UART1_DR;
		(void)dummy;
	}
}

uint16_t BL_CRC16(){
	uint16_t crc = 0x0000;
	uint16_t polynomial = 0x1021; //x^16 + x^12 + x^5 + 1

	uint8_t header[2] = {bl_cmd, bl_length};

    for (int i = 0; i < 2 + bl_length; i++){
        uint8_t byte;

        if (i == 0) byte = header[0];//bl_cmd;
        else if (i == 1) byte = header[1];//bl_length;
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

// [Start of Frame]     [CMD]     [Length]     [Data]     [CRC]
// 2 bytes 0xAA 0xBB   1 byte      1 byte   length bytes 2 bytes
void BL_Get_Packet() {
    uint8_t byte;

    bl_buffer[0] = 0xAA;

    byte = UART_Receive(UART1);
    if (byte != 0xBB) return;
    bl_buffer[1] = 0xBB;

    bl_buffer[2] = UART_Receive(UART1);
    bl_buffer[3] = UART_Receive(UART1);

    if (bl_buffer[3] > 250) {
        UART_Transmit_Char(UART1, BL_OVERLOAD);
        return;
    }

    for (uint16_t i = 0; i < bl_buffer[3]; i++) {
        bl_buffer[4 + i] = UART_Receive(UART1);
    }

    bl_buffer[4 + bl_buffer[3]] = UART_Receive(UART1);
    bl_buffer[5 + bl_buffer[3]] = UART_Receive(UART1);
}
void BL_Packet_Handler() {
    if ((bl_buffer[0] == 0xAA) && (bl_buffer[1] == 0xBB)) {
        bl_cmd = bl_buffer[2];
        bl_length = bl_buffer[3];

        for (int i = 0; i < 1000; i++);

        uint16_t received_crc = (bl_buffer[4 + bl_length] << 8) | bl_buffer[5 + bl_length];
        if (BL_CRC16() == received_crc) {

            for (uint8_t i = 0; i < bl_length; i++) {
                bl_data[i] = bl_buffer[4 + i];
            }

            switch(bl_cmd) {
                case BL_START:
                    FLASH_Page_Pointer = APP_ADD_BASE;
                    UART_Transmit_Char(UART1, BL_ACK);
                    break;

                case BL_PROGRAM:
                    for (uint16_t i = 0; i < bl_length; i += 2) {
                        uint16_t flash_data;
                        if (i + 1 < bl_length) {
                            flash_data = (bl_data[i+1] << 8) | bl_data[i];
                        }
                        else {
                            flash_data = (0xFF << 8) | bl_data[i];
                        }
                        FLASH_Program(FLASH_Page_Pointer, flash_data);
                        FLASH_Page_Pointer += 2;
                    }
                    UART_Transmit_Char(UART1, BL_ACK);
                    break;

                case BL_ERASE:
                    {
                        uint8_t num_pages = bl_data[0];
                        for (uint8_t i = 0; i < num_pages; i++) {
                            FLASH_Erase_Page(APP_ADD_BASE + (i * 1024));
                        }
                    }
                    UART_Transmit_Char(UART1, BL_ACK);
                    break;

                case BL_JUMP:
                    UART_Transmit_Char(UART1, BL_ACK);
                    for(volatile int d = 0; d < 500000; d++);
                    BL_JumpToApplication();
                    break;

                default:
                    UART_Transmit_Char(UART1, BL_NACK);
                    break;
            }
        } else {
            UART_Transmit_Char(UART1, BL_NACK);
        }
    } else {
        UART_Transmit_Char(UART1, BL_NACK);
    }
}
void BL_JumpToBootloader(void){
    uint32_t app_stack;
    uint32_t app_reset;
    pFunction JumpToBL;

    _Interrupts_Disable();

    STK_Disable();
    NVIC_Disable();

    SCB_VTOR = BL_ADD_BASE;

    app_stack = *(volatile uint32_t*)BL_ADD_BASE;

    app_reset = *(volatile uint32_t*)(BL_ADD_BASE + 4);

    _Set_MSP(app_stack);

    JumpToBL = (pFunction)app_reset;

    JumpToBL();
}
void BL_JumpToApplication(void){
    uint32_t app_stack;
    uint32_t app_reset;
    pFunction JumpToApp;

    _Interrupts_Disable();

    STK_Disable();
    NVIC_Disable();

    SCB_VTOR = APP_ADD_BASE;

    app_stack = *(volatile uint32_t*)APP_ADD_BASE;

    app_reset = *(volatile uint32_t*)(APP_ADD_BASE + 4);

    _Set_MSP(app_stack);

    JumpToApp = (pFunction)app_reset;

    JumpToApp();
}
void BL_Update_Verify(void){
    if (bl_update_signal == 0) return;

    uint8_t received_byte = bl_update_signal;
    bl_update_signal = 0;

    switch (bl_wait_key) {
        case BL_WAIT_KEY_1:
            if (received_byte == 0xCC) {
                bl_wait_key = BL_WAIT_KEY_2;
                second_wait_count = 0;
                UART_Transmit_Char(UART1, BL_ACK);
            }
            break;

        case BL_WAIT_KEY_2:
            if (received_byte == 0xDD) {
                bl_wait_key = BL_WAIT_KEY_VERIFIED;
                UART_Transmit_Char(UART1, BL_ACK);
            }
            else{
                bl_wait_key = BL_WAIT_KEY_1;
                UART_Transmit_Char(UART1, BL_NACK);
            }
            break;

        default:
            break;
    }
}
