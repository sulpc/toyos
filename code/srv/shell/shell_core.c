#include "shell_core.h"
#include "log_core.h"
#include "shell_cfg.h"
#include "util_fifo_buffer.h"

#include <ctype.h>   // use isalnum/isblank...
#include <string.h>


typedef struct {
    char              buffer[CMDLINE_LEN_MAX];
    uint8_t           length;
    bool              valid;
    int               argc;
    char*             argv[CMD_ARG_NUM_MAX];
    shell_cmd_entry_t entry;
} shell_cmdline_t;


#define CMD_PROMPT            "# "
#define CMD_CHK_OK            0
#define CMD_CHK_EMPTY         1   // blank line
#define CMD_CHK_INVALID_CMD   2   // cmd not find
#define CMD_CHK_TOO_MANY_ARGS 3   //
#define shell_printf(...)     log_printf(__VA_ARGS__), log_printf("%s", CMD_PROMPT)


static uint8_t shell_cmdline_parse(shell_cmdline_t* cmdline);


static uint8_t         shell_input_buffer[SHELL_BUFFER_SIZE];
static fifo_buffer_t   shell_fifo_buffer;
static shell_cmdline_t shell_cmdline;


/**
 * @brief
 *
 */
void shell_init(void) {
    memset(shell_input_buffer, 0, sizeof(shell_input_buffer));
    fifo_buffer_init(&shell_fifo_buffer, shell_input_buffer, sizeof(shell_input_buffer));
    memset(&shell_cmdline, 0, sizeof(shell_cmdline));
    shell_cmdline.valid = true;
}


/**
 * @brief
 *
 */
void shell_proc(void) {
    uint8_t temp_char;

    while (!fifo_buffer_empty(&shell_fifo_buffer)) {
        if (shell_cmdline.length >= CMDLINE_LEN_MAX) {
            // cmdline has invalid length
            shell_cmdline.valid  = false;
            shell_cmdline.length = 0;
        }

        fifo_buffer_get(&shell_fifo_buffer, &temp_char);
        if (temp_char == '\n') {
            if (shell_cmdline.valid == true) {
                // parse and exec cmdline
                shell_cmdline.buffer[shell_cmdline.length] = '\0';

                switch (shell_cmdline_parse(&shell_cmdline)) {
                case CMD_CHK_OK:
                    shell_cmdline.entry(shell_cmdline.argc, shell_cmdline.argv);
                    // ("cmd return %d.\n", cmd_ret);
                    shell_printf("");
                    break;
                case CMD_CHK_EMPTY:
                    shell_printf("");
                    break;
                case CMD_CHK_INVALID_CMD:
                    shell_printf("ERROR: invalid cmd!\n");
                    break;
                case CMD_CHK_TOO_MANY_ARGS:
                    shell_printf("ERROR: too many args (should less than 10)!\n");
                    break;
                default:
                    break;
                }

                shell_cmdline.buffer[0] = '\0';   // clear buffer
                shell_cmdline.length    = 0;

                break;   // exec only one cmd in a period
            } else {
                shell_printf("ERROR: cmdline too long!\n");
                // discard invalid cmdline
                shell_cmdline.valid  = true;
                shell_cmdline.length = 0;
            }
        } else if (isprint(temp_char))   // print char
        {
            shell_cmdline.buffer[shell_cmdline.length] = (char)temp_char;
            shell_cmdline.length++;
        } else if (temp_char == '\t') {
            shell_cmdline.buffer[shell_cmdline.length] = ' ';
            shell_cmdline.length++;
        } else if (temp_char == '\b')   // delete
        {
            if (shell_cmdline.length > 0) {
                shell_cmdline.length--;
            }
        } else {
            // ignore other data
        }
    }
}


/**
 * @brief
 *
 * @param data
 */
void shell_get_newchar(uint8_t data) {
    if (!fifo_buffer_full(&shell_fifo_buffer)) {
        fifo_buffer_put(&shell_fifo_buffer, data);
    }
}


/**
 * @brief
 *
 * @param argc
 * @param argv
 * @return int
 */
int shell_help_info(int argc, char* argv[]) {
    uint16_t i;
    for (i = 0; i < shell_cfg.cmd_num; i++) {
        log_printf("%-10s %s\n", shell_cfg.cmd_list[i].name, shell_cfg.cmd_list[i].info);
    }
    return 0;
}


/**
 * @brief
 *
 * @param cmdline
 * @return uint8_t
 */
static uint8_t shell_cmdline_parse(shell_cmdline_t* cmdline) {
    char* pchar = cmdline->buffer;

    cmdline->argc  = 0;
    cmdline->entry = nullptr;

    while (*pchar != '\0') {
        if (cmdline->argc >= CMD_ARG_NUM_MAX) {
            break;
        }
        // jump blanks
        while (*pchar == ' ') {
            pchar++;
        }
        if (*pchar == '\0') {
            break;
        }
        cmdline->argv[cmdline->argc] = pchar;
        cmdline->argc++;

        while (*pchar != ' ' && *pchar != '\0') {
            pchar++;
        }
        if (*pchar == ' ') {
            *pchar = '\0';
            pchar++;
        }
    }

    if (cmdline->argc == 0) {
        return CMD_CHK_EMPTY;
    } else if (cmdline->argc >= CMD_ARG_NUM_MAX) {
        return CMD_CHK_TOO_MANY_ARGS;
    } else {
        uint16_t idx;

        // search cmd
        for (idx = 0; idx < shell_cfg.cmd_num; idx++) {
            if (strcmp(cmdline->argv[0], shell_cfg.cmd_list[idx].name) == 0) {
                cmdline->entry = shell_cfg.cmd_list[idx].entry;
                break;
            }
        }

        return (idx == shell_cfg.cmd_num) ? CMD_CHK_INVALID_CMD : CMD_CHK_OK;
    }
}
