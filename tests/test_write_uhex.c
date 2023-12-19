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

static char write_results[256];
static char ack_results[256];

static uint8_t var_8;
static uint8_t var_8_index=0;
static uint8_t var_8_array[3];
static uint16_t var_16;
static uint16_t var_16_index=0;
static uint16_t var_16_array[3];
static uint32_t var_32;
static uint32_t var_32_index=0;
static uint32_t var_32_array[3];

static char const *input_text;
static size_t input_index;

static int cmd_write(const struct cat_command *cmd, const uint8_t *data, const size_t data_size, const size_t args_num)
{
        strcat(write_results, " CMD:");
        strncat(write_results, data, data_size);
        return 0;
}

static int var_first(const struct cat_variable *var, size_t write_size)
{
        var_8_array[var_8_index++] = var_8;
        return 0;
}

static int var_second(const struct cat_variable *var, size_t write_size)
{
        var_16_array[var_16_index++] = var_16;
        return 0;
}

static int var_third(const struct cat_variable *var, size_t write_size)
{
        var_32_array[var_32_index++] = var_32;
        return 0;
}

static struct cat_variable vars_triple[] = {
        {
                .type = CAT_VAR_UINT_HEX,
                .data = &var_8,
                .data_size = sizeof(var_8),
                .write = var_first,
                .name = "var_8",
                .access = CAT_VAR_ACCESS_READ_WRITE
        }, {
                .type = CAT_VAR_UINT_HEX,
                .data = &var_16,
                .data_size = sizeof(var_16),
                .write = var_second,
                .name = "var_16",
                .access = CAT_VAR_ACCESS_READ_WRITE
        }, {
                .type = CAT_VAR_UINT_HEX,
                .data = &var_32,
                .data_size = sizeof(var_32),
                .write = var_third,
                .name = "var_32",
                .access = CAT_VAR_ACCESS_READ_WRITE
        }
};

static struct cat_command cmds[] = {
        {
                .name = "+TRP",
                .write = cmd_write,

                .var = vars_triple,
                .var_num = sizeof(vars_triple) / sizeof(vars_triple[0]),
                .need_all_vars = false
        }
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

        memset(var_8_array, 0, sizeof(var_8_array));
        memset(var_16_array, 0, sizeof(var_16_array));
        memset(var_32_array, 0, sizeof(var_32_array));
        var_8_index = 0;
        var_16_index = 0;
        var_32_index = 0;

        memset(ack_results, 0, sizeof(ack_results));
        memset(write_results, 0, sizeof(write_results));
}

static const char test_case_1[] = "\nAT+TRP=0\nAT+TRP=aa\nAT+TRP=001\nAT+TRP=0x123\nAT+TRP=0x12\nAT+TRP=,1\n";
static const char test_case_2[] = "\nAT+TRP=,0\nAT+TRP=,aa\nAT+TRP=,001\nAT+TRP=,0x12345\nAT+TRP=,0x123\nAT+TRP=,,1\n";
static const char test_case_3[] = "\nAT+TRP=,,0\nAT+TRP=,,aa\nAT+TRP=,,001\nAT+TRP=,,0x123456789\nAT+TRP=,,0x12345678\nAT+TRP=,,\n";
static const char test_case_4[] = "\nAT+TRP=0,,0\nAT+TRP=aa,,aa\nAT+TRP=001,,001\nAT+TRP=0x12,,0x123456789\nAT+TRP=0x12,,0x321\n";

int main(int argc, char **argv)
{
        struct cat_object at;

        cat_init(&at, &desc, &iface, NULL);

        prepare_input(test_case_1);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\nOK\n\nERROR\n\nOK\n\nERROR\n\nOK\n\nOK\n") == 0);
        assert(strcmp(write_results, " CMD:0 CMD:001 CMD:0x12 CMD:,1") == 0);

        assert(var_8_array[0] == 0);
        assert(var_8_array[1] == 1);
        assert(var_8_array[2] == 0x12);
        assert(var_16_array[0] == 1);
        assert(var_16_array[1] == 0);
        assert(var_16_array[2] == 0);
        assert(var_32_array[0] == 0);
        assert(var_32_array[1] == 0);
        assert(var_32_array[2] == 0);

        prepare_input(test_case_2);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\nOK\n\nERROR\n\nOK\n\nERROR\n\nOK\n\nOK\n") == 0);
        assert(strcmp(write_results, " CMD:,0 CMD:,001 CMD:,0x123 CMD:,,1") == 0);

        assert(var_8_array[0]   == 0);
        assert(var_16_array[0]  == 0);
        assert(var_32_array[0]  == 1);
        assert(var_8_array[1]   == 0);
        assert(var_16_array[1]  == 1);
        assert(var_32_array[1]  == 0);
        assert(var_8_array[2]   == 0);
        assert(var_16_array[2]  == 0x123);
        assert(var_32_array[2]  == 0);

        prepare_input(test_case_3);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\nOK\n\nERROR\n\nOK\n\nERROR\n\nOK\n\nERROR\n") == 0);
        assert(strcmp(write_results, " CMD:,,0 CMD:,,001 CMD:,,0x12345678") == 0);

        assert(var_8_array[0]   == 0);
        assert(var_16_array[0]  == 0);
        assert(var_32_array[0]  == 0);
        assert(var_8_array[1]   == 0);
        assert(var_16_array[1]  == 0);
        assert(var_32_array[1]  == 1);
        assert(var_8_array[2]   == 0);
        assert(var_16_array[2]  == 0);
        assert(var_32_array[2]  == 0x12345678);

        prepare_input(test_case_4);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\nOK\n\nERROR\n\nOK\n\nERROR\n\nOK\n") == 0);
        assert(strcmp(write_results, " CMD:0,,0 CMD:001,,001 CMD:0x12,,0x321") == 0);

        assert(var_8_array[0]   == 0);
        assert(var_16_array[0]  == 0);
        assert(var_32_array[0]  == 0);
        assert(var_8_array[1]   == 1);
        assert(var_16_array[1]  == 0);
        assert(var_32_array[1]  == 1);
        assert(var_8_array[2]   == 0x12);
        assert(var_16_array[2]  == 0);
        assert(var_32_array[2]  == 0x321);

        return 0;
}
