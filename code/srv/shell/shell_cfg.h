#ifndef _CMD_CFG_H_
#define _CMD_CFG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "util_types.h"

#define CMDLINE_LEN_MAX   32
#define CMD_ARG_NUM_MAX   10
#define SHELL_BUFFER_SIZE 128

typedef int (*shell_cmd_entry_t)(int argc, char* argv[]);

typedef struct {
    const char*       name;
    shell_cmd_entry_t entry;
    const char*       info;
} shell_cmd_cfg_t;

typedef struct {
    const shell_cmd_cfg_t* cmd_list;
    const uint16_t         cmd_num;
} shell_cfg_t;


extern shell_cfg_t shell_cfg;


#ifdef __cplusplus
}
#endif

#endif   // _SHELL_CFG_H_
