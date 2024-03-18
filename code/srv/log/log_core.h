#ifndef _LOG_CORE_H_
#define _LOG_CORE_H_


#include "util_types.h"


#define LOG_ON  true
#define LOG_OFF false


typedef enum {
    LOG_INFO = 0,
    LOG_DEBUG,
    LOG_WARNING,
    LOG_ERROR,
    LOG_EXIT,
    LOG_LEVEL_NUM,
} log_level_t;

#define LOG_LEVEL_PREFIXS "INF: ", "DBG: ", "WRN: ", "ERR: ", "EXT: "

void log_init(void);
void log_proc(void);

// printf -- print log async, use buffer
void log_printf(const char* fmt, ...);
void log_printfx(bool log_sw, log_level_t log_level, const char* fmt, ...);

// printk -- print sync immdiately
void log_printk(const char* fmt, ...);
void log_printkx(bool log_sw, log_level_t log_level, const char* fmt, ...);

// log level set
void log_set_sys_level(log_level_t log_level);
void log_set_sys_enable(bool on_off);

#endif
