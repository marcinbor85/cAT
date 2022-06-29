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

static char ack_results[256];

static int8_t var_int;
static uint8_t var_uint;
static uint8_t var_hex8;
static uint16_t var_hex16;
static uint32_t var_hex32;
static uint8_t var_buf[4];
static char var_string[16];

static char const *input_text;
static size_t input_index;
static int common_cntr;

static int cmd_read(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size)
{
        return 0;
}

static int cmd2_read(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size)
{
        sprintf(data, "%s=test", cmd->name);
        *data_size = strlen(data);
        return 0;
}

static int common_var_read_handler(const struct cat_variable *var)
{
        common_cntr++;
        return 0;
}

static struct cat_variable vars[] = {
        {
                .type = CAT_VAR_INT_DEC,
                .data = &var_int,
                .data_size = sizeof(var_int),
                .read = common_var_read_handler
        },
        {
                .type = CAT_VAR_UINT_DEC,
                .data = &var_uint,
                .data_size = sizeof(var_uint),
                .read = common_var_read_handler
        },
        {
                .type = CAT_VAR_NUM_HEX,
                .data = &var_hex8,
                .data_size = sizeof(var_hex8),
                .read = common_var_read_handler
        },
        {
                .type = CAT_VAR_NUM_HEX,
                .data = &var_hex16,
                .data_size = sizeof(var_hex16),
                .read = common_var_read_handler
        },
        {
                .type = CAT_VAR_NUM_HEX,
                .data = &var_hex32,
                .data_size = sizeof(var_hex32),
                .read = common_var_read_handler
        },
        {
                .type = CAT_VAR_BUF_HEX,
                .data = &var_buf,
                .data_size = sizeof(var_buf),
                .read = common_var_read_handler
        },
        {
                .type = CAT_VAR_BUF_STRING,
                .data = &var_string,
                .data_size = sizeof(var_string),
                .read = common_var_read_handler
        }
};

static struct cat_command cmds[] = {
        {
                .name = "+SET",
                .read = cmd_read,

                .var = vars,
                .var_num = sizeof(vars) / sizeof(vars[0])
        },
        {
                .name = "+TEST",
                .read = cmd2_read,

                .var = vars,
                .var_num = sizeof(vars) / sizeof(vars[0])
        },
};

static char buf[128];

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
        .buf_size = sizeof(buf)
};

static int write_char(char ch)
{
        char str[2];
        str[0] = ch;
        str[1] = 0;
        strcat(ack_results, str);
        return 1;
}

static int read_char(char *ch)
{
        if (input_index >= strlen(input_text))
                return 0;

        *ch = input_text[input_index];
        input_index++;
        return 1;
}

static struct cat_io_interface iface = {
        .read = read_char,
        .write = write_char
};

static void prepare_input(const char *text)
{
        input_text = text;
        input_index = 0;

        var_int = -1;
        var_uint = 255;
        var_hex8 = 0xAA;
        var_hex16 = 0x0123;
        var_hex32 = 0xFF001234;

        var_buf[0] = 0x12;
        var_buf[1] = 0x34;
        var_buf[2] = 0x56;
        var_buf[3] = 0x78;

        common_cntr = 0;

        sprintf(var_string, "\\\"test\n");

        memset(ack_results, 0, sizeof(ack_results));
}

static const char test_case_1[] = "\nAT+SET?\r\n";
static const char test_case_2[] = "\nAT+TEST?\n";

int main(int argc, char **argv)
{
        struct cat_object at;

        cat_init(&at, &desc, &iface, NULL);

        prepare_input(test_case_1);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\r\n+SET=-1,255,0xAA,0x0123,0xFF001234,12345678,\"\\\\\\\"test\\n\"\r\n\r\nOK\r\n") == 0);
        assert(common_cntr == 7);

        prepare_input(test_case_2);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\n+TEST=test\n\nOK\n") == 0);
        assert(common_cntr == 7);

        return 0;
}
