/*
MIT License

Copyright (c) 2019 Marcin Borowicz

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <assert.h>

#include "../src/cat.h"

/* variables assigned to scan command */
static int rssi;
static char ssid[16];

/* helper variable used to exit demo code */
static bool quit_flag;

/* variables assigned to start command */
static int mode;

/* main at command parser object */
struct cat_object at;

struct scan_results {
        int rssi;
        char ssid[16];
};

/* static const scan results */
static const struct scan_results results[2][3] = {
        {
                {
                        .rssi = -10,
                        .ssid = "wifi1",
                },
                {
                        .rssi = -50,
                        .ssid = "wifi2",
                },
                {
                        .rssi = -20,
                        .ssid = "wifi3",
                }
        },
        {
                {
                        .rssi = -20,
                        .ssid = "bluetooth1",
                }
        }
};

/* helper variable */
static int scan_index;

/* run command handler with custom exit mechanism */
static int quit_run(const struct cat_command *cmd)
{
        quit_flag = true;
        return 0;
}

/* helper function */
static void load_scan_results(int index)
{
        rssi = results[mode][index].rssi;
        strcpy(ssid, results[mode][index].ssid);
}

/* declaring scan variables array */
static struct cat_variable scan_vars[] = {
        {
                .type = CAT_VAR_INT_DEC,
                .data = &rssi,
                .data_size = sizeof(rssi),
                .name = "RSSI"
        },
        {
                .type = CAT_VAR_BUF_STRING,
                .data = ssid,
                .data_size = sizeof(ssid),
                .name = "SSID"
        }
};

/* forward declaration */
static cat_return_state scan_read(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size);

/* unsolicited command */
static struct cat_command scan_cmd = {
        .name = "+SCAN",
        .read = scan_read,
        .var = scan_vars,
        .var_num = sizeof(scan_vars) / sizeof(scan_vars[0])
};

/* unsolicited read callback handler */
static cat_return_state scan_read(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size)
{
        int max = (mode == 0) ? 3 : 1;

        scan_index++;
        if (scan_index > max)
                return CAT_RETURN_STATE_HOLD_EXIT_OK;

        load_scan_results(scan_index);
        cat_trigger_unsolicited_read(&at, &scan_cmd);

        return CAT_RETURN_STATE_DATA_NEXT;
}

/* mode variable validator */
static int mode_write(const struct cat_variable *var, const size_t write_size)
{
        if (*(int*)var->data >= 2)
                return -1;
        return 0;
}

/* start write callback handler */
static cat_return_state start_write(const struct cat_command *cmd, const uint8_t *data, const size_t data_size, const size_t args_num)
{
        scan_index = 0;

        load_scan_results(scan_index);
        cat_trigger_unsolicited_read(&at, &scan_cmd);

        return CAT_RETURN_STATE_HOLD;
}

/* run command handler attached to HELP command for printing commands list */
static int print_cmd_list(const struct cat_command *cmd)
{
        return CAT_RETURN_STATE_PRINT_CMD_LIST_OK;
}

/* declaring start command variables array */
static struct cat_variable start_vars[] = {
        {
                .type = CAT_VAR_UINT_DEC,
                .data = &mode,
                .data_size = sizeof(mode),
                .name = "MODE",
                .write = mode_write,
                .access = CAT_VAR_ACCESS_WRITE_ONLY,
        }
};

/* declaring commands array */
static struct cat_command cmds[] = {
        {
                .name = "+START",
                .description = "Start scanning after write (0 - wifi, 1 - bluetooth).",
                .write = start_write,
                .var = start_vars,
                .var_num = sizeof(start_vars) / sizeof(start_vars[0]),
                .need_all_vars = true
        },
        {
                .name = "+SCAN",
                .description = "Scan result record.",
                .only_test = true,
                .var = scan_vars,
                .var_num = sizeof(scan_vars) / sizeof(scan_vars[0])
        },
        {
                .name = "#HELP",
                .run = print_cmd_list,
        },
        {
                .name = "#QUIT",
                .run = quit_run
        },
};

/* working buffer */
static char buf[128];

/* declaring parser descriptor */
static struct cat_command_group cmd_group = {
        .cmd = cmds,
        .cmd_num = sizeof(cmds) / sizeof(cmds[0]),
};

static struct cat_command_group *cmd_desc[] = {
        &cmd_group
};

static struct cat_descriptor desc = {
        .cmd_group = cmd_desc,
        .cmd_group_num = sizeof(cmd_desc) / sizeof(cmd_desc[0]),

        .buf = buf,
        .buf_size = sizeof(buf),
};

/* custom target dependent input output handlers */
static int write_char(char ch)
{
        putc(ch, stdout);
        return 1;
}

static int read_char(char *ch)
{
        *ch = getc(stdin);
        return 1;
}

/* declaring input output interface descriptor for parser */
static struct cat_io_interface iface = {
        .read = read_char,
        .write = write_char
};

int main(int argc, char **argv)
{
        /* initializing */
        cat_init(&at, &desc, &iface, NULL);

        /* main loop with exit code conditions */
        while ((cat_service(&at) != 0) && (quit_flag == 0)) {};

        /* goodbye message */
        printf("Bye!\n");

        return 0;
}
