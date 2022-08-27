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

static int8_t var_int8;
static int16_t var_int16;
static int32_t var_int32;
static uint8_t var_uint8;
static uint16_t var_uint16;
static uint32_t var_uint32;
static uint8_t var_hex8;
static uint16_t var_hex16;
static uint32_t var_hex32;
static uint8_t var_buf[4];
static char var_string[16];

static char const *input_text;
static size_t input_index;

static int cmd_override_test(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size)
{
        strcat(data, "\ntest");
        *data_size = strlen(data);
        return 0;
}

static int cmd_error_test(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size)
{
        return -1;
}

static int cmd_ok_test(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size)
{
        strcpy(data, "test1");
        *data_size = strlen(data);
        return 0;
}

static int cmd_ok2_test(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size)
{
        strcat(data, "test2");
        *data_size = strlen(data);
        return 0;
}

static struct cat_variable vars[] = {
        {
                .type = CAT_VAR_INT_DEC,
                .data = &var_int8,
                .data_size = sizeof(var_int8),
                .name = "x"
        },
        {
                .type = CAT_VAR_INT_DEC,
                .data = &var_int16,
                .data_size = sizeof(var_int16),
                .name = "y"
        },
        {
                .type = CAT_VAR_INT_DEC,
                .data = &var_int32,
                .data_size = sizeof(var_int32)
        },
        {
                .type = CAT_VAR_UINT_DEC,
                .data = &var_uint8,
                .data_size = sizeof(var_uint8)
        },
        {
                .type = CAT_VAR_UINT_DEC,
                .data = &var_uint16,
                .data_size = sizeof(var_uint16)
        },
        {
                .type = CAT_VAR_UINT_DEC,
                .data = &var_uint32,
                .data_size = sizeof(var_uint32)
        },
        {
                .type = CAT_VAR_NUM_HEX,
                .data = &var_hex8,
                .data_size = sizeof(var_hex8)
        },
        {
                .type = CAT_VAR_NUM_HEX,
                .data = &var_hex16,
                .data_size = sizeof(var_hex16)
        },
        {
                .type = CAT_VAR_NUM_HEX,
                .data = &var_hex32,
                .data_size = sizeof(var_hex32)
        },
        {
                .type = CAT_VAR_BUF_HEX,
                .data = &var_buf,
                .data_size = sizeof(var_buf)
        },
        {
                .type = CAT_VAR_BUF_STRING,
                .data = &var_string,
                .data_size = sizeof(var_string),
                .name = "msg"
        }
};

static struct cat_variable vars_ro[] = {
        {
                .type = CAT_VAR_INT_DEC,
                .data = &var_int8,
                .data_size = sizeof(var_int8),
                .name = "x",
                .access = CAT_VAR_ACCESS_READ_ONLY
        },
        {
                .type = CAT_VAR_INT_DEC,
                .data = &var_int16,
                .data_size = sizeof(var_int16),
                .name = "y",
                .access = CAT_VAR_ACCESS_READ_ONLY
        },
        {
                .type = CAT_VAR_INT_DEC,
                .data = &var_int32,
                .data_size = sizeof(var_int32),
                .access = CAT_VAR_ACCESS_READ_ONLY
        },
        {
                .type = CAT_VAR_UINT_DEC,
                .data = &var_uint8,
                .data_size = sizeof(var_uint8),
                .access = CAT_VAR_ACCESS_READ_ONLY
        },
        {
                .type = CAT_VAR_UINT_DEC,
                .data = &var_uint16,
                .data_size = sizeof(var_uint16),
                .access = CAT_VAR_ACCESS_READ_ONLY
        },
        {
                .type = CAT_VAR_UINT_DEC,
                .data = &var_uint32,
                .data_size = sizeof(var_uint32),
                .access = CAT_VAR_ACCESS_READ_ONLY
        },
        {
                .type = CAT_VAR_NUM_HEX,
                .data = &var_hex8,
                .data_size = sizeof(var_hex8),
                .access = CAT_VAR_ACCESS_READ_ONLY
        },
        {
                .type = CAT_VAR_NUM_HEX,
                .data = &var_hex16,
                .data_size = sizeof(var_hex16),
                .access = CAT_VAR_ACCESS_READ_ONLY
        },
        {
                .type = CAT_VAR_NUM_HEX,
                .data = &var_hex32,
                .data_size = sizeof(var_hex32),
                .access = CAT_VAR_ACCESS_READ_ONLY
        },
        {
                .type = CAT_VAR_BUF_HEX,
                .data = &var_buf,
                .data_size = sizeof(var_buf),
                .access = CAT_VAR_ACCESS_READ_ONLY
        },
        {
                .type = CAT_VAR_BUF_STRING,
                .data = &var_string,
                .data_size = sizeof(var_string),
                .name = "msg",
                .access = CAT_VAR_ACCESS_READ_ONLY
        }
};

static struct cat_variable vars_wo[] = {
        {
                .type = CAT_VAR_INT_DEC,
                .data = &var_int8,
                .data_size = sizeof(var_int8),
                .name = "x",
                .access = CAT_VAR_ACCESS_WRITE_ONLY
        },
        {
                .type = CAT_VAR_INT_DEC,
                .data = &var_int16,
                .data_size = sizeof(var_int16),
                .name = "y",
                .access = CAT_VAR_ACCESS_WRITE_ONLY
        },
        {
                .type = CAT_VAR_INT_DEC,
                .data = &var_int32,
                .data_size = sizeof(var_int32),
                .access = CAT_VAR_ACCESS_WRITE_ONLY
        },
        {
                .type = CAT_VAR_UINT_DEC,
                .data = &var_uint8,
                .data_size = sizeof(var_uint8),
                .access = CAT_VAR_ACCESS_WRITE_ONLY
        },
        {
                .type = CAT_VAR_UINT_DEC,
                .data = &var_uint16,
                .data_size = sizeof(var_uint16),
                .access = CAT_VAR_ACCESS_WRITE_ONLY
        },
        {
                .type = CAT_VAR_UINT_DEC,
                .data = &var_uint32,
                .data_size = sizeof(var_uint32),
                .access = CAT_VAR_ACCESS_WRITE_ONLY
        },
        {
                .type = CAT_VAR_NUM_HEX,
                .data = &var_hex8,
                .data_size = sizeof(var_hex8),
                .access = CAT_VAR_ACCESS_WRITE_ONLY
        },
        {
                .type = CAT_VAR_NUM_HEX,
                .data = &var_hex16,
                .data_size = sizeof(var_hex16),
                .access = CAT_VAR_ACCESS_WRITE_ONLY
        },
        {
                .type = CAT_VAR_NUM_HEX,
                .data = &var_hex32,
                .data_size = sizeof(var_hex32),
                .access = CAT_VAR_ACCESS_WRITE_ONLY
        },
        {
                .type = CAT_VAR_BUF_HEX,
                .data = &var_buf,
                .data_size = sizeof(var_buf),
                .access = CAT_VAR_ACCESS_WRITE_ONLY
        },
        {
                .type = CAT_VAR_BUF_STRING,
                .data = &var_string,
                .data_size = sizeof(var_string),
                .name = "msg",
                .access = CAT_VAR_ACCESS_WRITE_ONLY
        }
};

static struct cat_variable vars2[] = {
        {
                .type = CAT_VAR_INT_DEC,
                .data = &var_int8,
                .data_size = sizeof(var_int8),
                .name = "var"
        }
};

static struct cat_command cmds[] = {
        {
                .name = "+SET",

                .var = vars,
                .var_num = sizeof(vars) / sizeof(vars[0])
        },
        {
                .name = "+SETRO",

                .var = vars_ro,
                .var_num = sizeof(vars_ro) / sizeof(vars_ro[0])
        },
        {
                .name = "+SETWO",

                .var = vars_wo,
                .var_num = sizeof(vars_wo) / sizeof(vars_wo[0])
        },
        {
                .name = "+TEST",
                .description = "test_desc",
                .test = cmd_override_test,

                .var = vars2,
                .var_num = sizeof(vars2) / sizeof(vars2[0])
        },
        {
                .name = "+TEST2",
                .description = "test2_desc",

                .var = vars2,
                .var_num = sizeof(vars2) / sizeof(vars2[0])
        },
        {
                .name = "+AP",
                .test = cmd_error_test,

                .var = vars2,
                .var_num = sizeof(vars2) / sizeof(vars2[0])
        },
        {
                .name = "+ZZ",
                .test = cmd_ok_test,
        },
        {
                .name = "+ZZ2",
                .description = "zz2_desc",
                .test = cmd_ok_test,
        },
        {
                .name = "+ZZ3",
                .description = "zz3_desc",
                .test = cmd_ok2_test,
        }
};

static char buf[256];
static char unsolicited_buf[256];

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
        .unsolicited_buf = unsolicited_buf,
        .unsolicited_buf_size = sizeof(unsolicited_buf),
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

        var_int8 = -8;
        var_int16 = -16;
        var_int32 = -32;
        var_uint8 = 8;
        var_uint8 = 16;
        var_uint8 = 32;
        var_hex8 = 0x08;
        var_hex16 = 0x16;
        var_hex32 = 0x32;
        var_hex16 = 0x0123;
        var_hex32 = 0xFF001234;

        var_buf[0] = 0x12;
        var_buf[1] = 0x34;
        var_buf[2] = 0x56;
        var_buf[3] = 0x78;

        sprintf(var_string, "TST");

        memset(ack_results, 0, sizeof(ack_results));
}

static const char test_case_1[] = "\nAT+SET=?\n";
static const char test_case_1_ro[] = "\nAT+SETRO=?\n";
static const char test_case_1_wo[] = "\nAT+SETWO=?\n";
static const char test_case_2[] = "\nAT+TEST=?\nAT+TEST2=?\r\nAT+AP=?\n";
static const char test_case_3[] = "\nAT+ZZ=?\nAT+ZZ2=?\nAT+ZZ3=?\r\n";

int main(int argc, char **argv)
{
        struct cat_object at;

        cat_init(&at, &desc, &iface, NULL);

        prepare_input(test_case_1);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\n+SET=<x:INT8[RW]>,<y:INT16[RW]>,<INT32[RW]>,<UINT8[RW]>,<UINT16[RW]>,<UINT32[RW]>,<HEX8[RW]>,<HEX16[RW]>,<HEX32[RW]>,<HEXBUF[RW]>,<msg:STRING[RW]>\n\nOK\n") == 0);

        prepare_input(test_case_1_ro);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\n+SETRO=<x:INT8[RO]>,<y:INT16[RO]>,<INT32[RO]>,<UINT8[RO]>,<UINT16[RO]>,<UINT32[RO]>,<HEX8[RO]>,<HEX16[RO]>,<HEX32[RO]>,<HEXBUF[RO]>,<msg:STRING[RO]>\n\nOK\n") == 0);

        prepare_input(test_case_1_wo);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\n+SETWO=<x:INT8[WO]>,<y:INT16[WO]>,<INT32[WO]>,<UINT8[WO]>,<UINT16[WO]>,<UINT32[WO]>,<HEX8[WO]>,<HEX16[WO]>,<HEX32[WO]>,<HEXBUF[WO]>,<msg:STRING[WO]>\n\nOK\n") == 0);

        prepare_input(test_case_2);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\n+TEST=<var:INT8[RW]>\ntest_desc\ntest\n\nOK\n\r\n+TEST2=<var:INT8[RW]>\r\ntest2_desc\r\n\r\nOK\r\n\nERROR\n") == 0);

        prepare_input(test_case_3);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\ntest1\n\nOK\n\ntest1\n\nOK\n\r\n+ZZ3=\r\nzz3_desctest2\r\n\r\nOK\r\n") == 0);

        return 0;
}
