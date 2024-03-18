#ifndef _APP_SHELL_H_
#define _APP_SHELL_H_

#include "util_types.h"


void shell_init(void);
void shell_proc(void);
void shell_get_newchar(uint8_t data);   // called in uart isr
int  shell_help_info(int argc, char* argv[]);

#endif
