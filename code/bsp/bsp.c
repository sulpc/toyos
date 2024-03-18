#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "bsp.h"
#include <stm32f10x.h>

#include "log_core.h"
#include "shell_core.h"
#include "tos_core.h"


static uint16_t delay_us_fac    = 0;
static uint8_t  group_prio_bits = 0;
uint32_t        SystemCoreClock = 0;
static bool     uart_dbg_inited = false;


/**
 * @brief init sysclk to 72 MHz
 *
 */
void sysclk_init(void) {
    unsigned char temp = 0;

    // CR: clock control
    RCC->CR |= 0x00010000;     // HSEON, use external HSE
    while (!(RCC->CR >> 17))   // wait HSERDY
        ;

    // CFGR: clock config
    RCC->CFGR = 0x00000400;            // APB1/2, APB2/1, AHB/1
    RCC->CFGR |= (72 / 8 - 2) << 18;   // set PLL
    RCC->CFGR |= 1 << 16;              // PLLSRC, set PLL source to HSE

    FLASH->ACR |= 0x32;   // FLASH access delay, ref: manual P60

    RCC->CR |= 0x01000000;     // PLLON  PLL enable
    while (!(RCC->CR >> 25))   // PLLRDY PLL ready
        ;
    RCC->CFGR |= 0x00000002;       // choose PLL as sysclk
    while ((temp & 0x03) != 2) {   // sysclk switch to PLL
        temp = RCC->CFGR >> 2;
    }

    SystemCoreClock = 72 * 1000000;
}


/**
 * @brief NVIC priority group config
 *
 * @param group_bits
 */
static void NVIC_PrioGroupCfg(uint8_t group_bits) {
    uint32_t temp;
    group_prio_bits = (group_bits > 4) ? 4 : group_bits;

    /**
     *  SCB_AIRCR[10:8]
     *    PRIGROUP[2:0]   GroupBits
     *                3 - 4
     *                4 - 3
     *                5 - 2
     *                6 - 1
     *                7 - 0
     */
    temp = SCB->AIRCR;                         //
    temp = (temp & 0x0000F8FF) | 0x05FA0000;   // write key
    temp |= (7 - group_prio_bits) << 8;        //
    SCB->AIRCR = temp;                         //
}


/**
 * @brief NVIC init
 *
 * @param GroupPrio
 * @param SubPrio
 * @param NvicChannel IRQ No.
 */
void NVIC_Init(uint8_t GroupPrio, uint8_t SubPrio, uint8_t NvicChannel) {
    // ISER: 8 regs, 240 bits
    NVIC->ISER[NvicChannel >> 5] |= 1 << (NvicChannel & 0x1F);   // enable irq

    GroupPrio = GroupPrio << (8 - group_prio_bits);
    SubPrio   = SubPrio << 4;

    // NVIC_IP[7:4] : GroupPrio+SubPrio
    NVIC->IP[NvicChannel] |= (GroupPrio | SubPrio) & 0xF0;
}


void sysirq_init(void) {
    NVIC_PrioGroupCfg(2);
}


void delay_init(void) {
    SysTick->CTRL &= ~(1 << 2);                 // bit2=0, set clk to AHB/8
    delay_us_fac = SystemCoreClock / 8000000;   //
}

void delay_us(uint32_t nus) {
    uint32_t temp;
    SysTick->LOAD = nus * delay_us_fac;         // load time value
    SysTick->VAL  = 0x00;                       // clear counter
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;   // start
    do {
        temp = SysTick->CTRL;
    } while ((temp & SysTick_CTRL_ENABLE_Msk) && !(temp & SysTick_CTRL_COUNTFLAG_Msk));   // wait

    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;   // close
    SysTick->VAL = 0x00;                         //
}

void delay_ms(uint32_t nms) {
    if (tos_running()) {
        tos_task_sleep(nms);
    } else {
        delay_us(nms * 1000);
    }
}


#define dbg_putc(c)                                                                                                    \
    do {                                                                                                               \
        USART1->DR = (c);                                                                                              \
        while ((USART1->SR & 0x0040) == 0)                                                                             \
            ;                                                                                                          \
    } while (0)


void uart_dbg_init(uint32_t bound) {
    float    usartDiv;
    uint16_t divMantissa;
    uint16_t divFraction;
    // integer part
    usartDiv    = (float)(SystemCoreClock) / (bound * 16);
    divMantissa = usartDiv;
    // fractional part
    usartDiv    = (usartDiv - divMantissa) * 16;
    divFraction = usartDiv;
    if ((usartDiv - divFraction) > 0.5)
        divFraction += 1;

    // PA9  TX Alternate function output Push-pull 0xB
    // PA10 RX Floating input                      0x4

    RCC->APB2ENR |= 1 << 2;                           // enable PORTA clock
    RCC->APB2ENR |= 1 << 14;                          // enable uart clock
    GPIOA->CRH &= 0xFFFFF00F;                         //
    GPIOA->CRH |= 0x000004B0;                         // config GPIOA[9:10]
    RCC->APB2RSTR |= 1 << 14;                         // reset uart1
    RCC->APB2RSTR &= ~(1 << 14);                      //
    USART1->BRR = (divMantissa << 4) | divFraction;   // set bound
    USART1->CR1 |= 0x200C;                            // enable uart, tx, rx
    NVIC_Init(3, 3, USART1_IRQn);                     // init uart1 irq
    USART1->CR1 |= 0x1u << 5;                         // enable uart RXNEIE

    uart_dbg_inited = true;
    dbg_putc('\0');
    log_printk("uart inited\n");
}


void uart_dbg_send_data(const uint8_t* data, uint16_t len) {
    if (uart_dbg_inited) {
        uint16_t idx = 0;
        for (idx = 0; idx < len; idx++) {
            dbg_putc(data[idx]);
        }
    }
}


void uart_dbg_send_string(const char* data) {
    if (uart_dbg_inited) {
        if (data == nullptr)
            return;
        while (*data != '\0') {
            dbg_putc(*data);
            data++;
        }
    }
}


void uart_dbg_isr(void) {
    // tos_enter_isr();

    if (USART1->SR & (0x1u << 5)) {   // RXNE
        uint8_t data = USART1->DR;
        shell_get_newchar(data);
    }

    // tos_exit_isr();
}
