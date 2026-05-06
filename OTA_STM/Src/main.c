#include "RCC.h"
#include "UART.h"
#include "STM_BOOTLOADER.h"

int main(void) {
    RCC_Enable(RCC_PORTA);
    GPIO_Config(GPIO_PORTA, 0, GPIO_CNF_IN_PU, GPIO_MODE_IN);
    for(volatile int i = 0; i < 50000; i++);

    if (GPIO_Read_Pin(GPIO_PORTA, 0) == 1) BL_JumpToApplication();

    RCC_Enable(RCC_UART1);
    UART_Init(UART1, 9600, UART_WORD_LENGTH_8);

    _Interrupts_Disable();
    while (1) {
        if (bl_wait_key != BL_WAIT_KEY_VERIFIED) {
            if (UART_HasData(UART1)) {
            	bl_update_signal = UART_Receive(UART1);
                BL_Update_Verify();
            }
        }
        else {
            if (UART_HasData(UART1)) {
                if (UART1_DR == 0xAA) {
                    BL_Get_Packet();
                    BL_Packet_Handler();
                }
            }
        }
    }
}
