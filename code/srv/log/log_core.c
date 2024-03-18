#include "bsp.h"   // bsp


#include "log_core.h"
#include "util_fifo_buffer.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>


#define LOG_BUFFER_SIZE        1024
#define LOG_LINE_BUFFER_SIZE   64
#define DEFAULT_SYS_LOG_LEVEL  LOG_ERROR
#define DEFAULT_SYS_LOG_SWITCH LOG_OFF


typedef int32_t (*log_tx_func_t)(uint8_t* msg, uint16_t msg_len);
typedef struct {
    log_level_t   log_level;
    bool          log_enable;
    log_tx_func_t tx_func;
    fifo_buffer_t fifo_buffer;
} channel_cfg_t;


static int32_t log_msg_tx(uint8_t* msg, uint16_t msg_len);


static char*         log_level_prefix[] = {LOG_LEVEL_PREFIXS};
static uint8_t       log_msg_buffer[LOG_BUFFER_SIZE];
static channel_cfg_t log_ch_cfg = {
    .log_level  = DEFAULT_SYS_LOG_LEVEL,
    .log_enable = DEFAULT_SYS_LOG_SWITCH,
    .tx_func    = log_msg_tx,
    .fifo_buffer =
        {
            .buffer      = log_msg_buffer,
            .buffer_size = sizeof(log_msg_buffer),
            .pos_rd      = 0,
            .pos_wr      = 0,
        },
};


/**
 * @brief
 *
 */
void log_init(void) {
    fifo_buffer_init(&log_ch_cfg.fifo_buffer, &log_msg_buffer[0], LOG_BUFFER_SIZE);
}


/**
 * @brief
 *
 */
void log_proc(void) {
    static char buffer[128];
    uint16_t    data_size;

    data_size = fifo_buffer_get_block(&log_ch_cfg.fifo_buffer, (uint8_t*)buffer, sizeof(buffer));
    log_ch_cfg.tx_func((uint8_t*)buffer, data_size);
}


/**
 * @brief
 *
 * @param fmt
 * @param ...
 */
void log_printf(const char* fmt, ...) {
    static char buffer[64];
    va_list     ap;

    // TODO
    // lock
    va_start(ap, fmt);
    vsprintf(buffer, fmt, ap);
    va_end(ap);

    fifo_buffer_put_block(&log_ch_cfg.fifo_buffer, (uint8_t*)buffer, (uint16_t)strlen(buffer));
    // uart_dbg_send_string(buffer);

    // unlock
}


/**
 * @brief
 *
 * @param log_sw
 * @param log_level
 * @param fmt
 * @param ...
 */
void log_printfx(bool log_sw, log_level_t log_level, const char* fmt, ...) {
    static char buffer[LOG_LINE_BUFFER_SIZE];
    va_list     ap;

    if (log_level >= LOG_LEVEL_NUM) {
        return;
    }

    if (log_sw == LOG_OFF || log_ch_cfg.log_enable == LOG_OFF || log_level < log_ch_cfg.log_level) {
        return;
    }

    // TODO:
    // lock
    // add prefix: time level
    // ...
    va_start(ap, fmt);
    vsprintf(buffer, fmt, ap);
    va_end(ap);

    // add into buffer
    fifo_buffer_put_block(&log_ch_cfg.fifo_buffer, (uint8_t*)buffer, (uint16_t)strlen(buffer));

    // unlock
}


/**
 * @brief log level set
 *
 * @param log_level
 */
void log_set_sys_level(log_level_t log_level) {
    if (log_level >= LOG_LEVEL_NUM) {
        return;
    }
    log_ch_cfg.log_level = log_level;
}


/**
 * @brief
 *
 * @param on_off
 */
void log_set_sys_enable(bool on_off) {
    log_ch_cfg.log_enable = (on_off == LOG_ON) ? LOG_ON : LOG_OFF;
}


/**
 * @brief sync print, for base test
 *
 * @param fmt
 * @param ...
 */
void log_printk(const char* fmt, ...) {
    static char buffer[LOG_LINE_BUFFER_SIZE];
    va_list     ap;

    va_start(ap, fmt);
    vsprintf(buffer, fmt, ap);
    va_end(ap);

    uart_dbg_send_string(buffer);
}


/**
 * @brief
 *
 * @param log_sw
 * @param log_level
 * @param fmt
 * @param ...
 */
void log_printkx(bool log_sw, log_level_t log_level, const char* fmt, ...) {
    static char buffer[LOG_LINE_BUFFER_SIZE];
    va_list     ap;

    if (log_sw != LOG_ON || log_level >= LOG_LEVEL_NUM)
        return;
    uart_dbg_send_string(log_level_prefix[log_level]);

    va_start(ap, fmt);
    vsprintf(buffer, fmt, ap);
    va_end(ap);

    uart_dbg_send_string(buffer);
}


/**
 * @brief
 *
 * @param msg
 * @param msg_len
 * @return int32_t
 */
static int32_t log_msg_tx(uint8_t* msg, uint16_t msg_len) {
    uart_dbg_send_data(msg, msg_len);
    return 0;
}
