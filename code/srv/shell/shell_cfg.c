#include "shell_cfg.h"
#include "log_core.h"
#include "shell_core.h"

extern int main_cmd(int argc, char* argv[]);

static int echo_cmd(int argc, char* argv[]) {
    int i = 0;
    for (i = 1; i < argc; i++) {
        log_printf("%s ", argv[i]);
    }
    log_printf("\n");
    return 0;
}


static const shell_cmd_cfg_t shell_cmd_list[] = {
    {"echo", echo_cmd, "usage: echo xxx"},
    {"help", shell_help_info, "usage: help"},
    {"main", main_cmd, "usage: main ..."},
};


shell_cfg_t shell_cfg = {
    shell_cmd_list,
    sizeof(shell_cmd_list) / sizeof(shell_cmd_list[0]),
};
