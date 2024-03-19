#ifndef _STM32_DRV_H_
#define _STM32_DRV_H_


#include "util_types.h"


// SYSCLK
void sysclk_init(void);

// IRQ
void sysirq_init(void);
void NVIC_Init(uint8_t GroupPrio, uint8_t SubPrio, uint8_t NvicChannel);

#if 0
// DELAY -- conflict with os
void delay_init(void);
void delay_us(uint32_t nus);
void delay_ms(uint32_t nms);
#endif

// UART
void uart_dbg_init(uint32_t bound);
void uart_dbg_send_data(const uint8_t* data, uint16_t len);
void uart_dbg_send_string(const char* data);
void uart_dbg_isr(void);


#endif
