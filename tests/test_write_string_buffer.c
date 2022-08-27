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

static uint8_t var[8];
static size_t var_write_size[4];
static int var_write_size_index;

static char const *input_text;
static size_t input_index;

static int cmd_write(const struct cat_command *cmd, const uint8_t *data, const size_t data_size, const size_t args_num)
{
        strcat(write_results, " CMD:");
        strncat(write_results, data, data_size);
        return 0;
}

static int var_write(const struct cat_variable *var, size_t write_size)
{
        var_write_size[var_write_size_index++] = write_size;
        return 0;
}

static struct cat_variable vars[] = {
        {
                .type = CAT_VAR_BUF_STRING,
                .data = var,
                .data_size = sizeof(var),
                .write = var_write
        }
};

static struct cat_command cmds[] = {
        {
                .name = "+SET",
                .write = cmd_write,

                .var = vars,
                .var_num = sizeof(vars) / sizeof(vars[0])
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

        memset(var, 0, sizeof(var));
        memset(var_write_size, 0, sizeof(var_write_size));
        var_write_size_index = 0;

        memset(ack_results, 0, sizeof(ack_results));
        memset(write_results, 0, sizeof(write_results));
}

static const char test_case_1[] = "\nAT+SET=0\nAT+SET=\"\\\"abcd\\\"\"\nAT+SET=\"\"a\nAT+SET=\"1122334\"\nAT+SET=\"t\"\r\n";
static const char test_case_2[] = "\nAT+SET=\"12345678\"\nAT+SET=\"\"\nAT+SET=\"\\\\\\\\\"\nAT+SET=\"r1\\nr2\\n\"\n";

int main(int argc, char **argv)
{
        struct cat_object at;

        cat_init(&at, &desc, &iface, NULL);

        prepare_input(test_case_1);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\nERROR\n\nOK\n\nERROR\n\nOK\n\r\nOK\r\n") == 0);
        assert(strcmp(write_results, " CMD:\"\\\"abcd\\\"\" CMD:\"1122334\" CMD:\"t\"") == 0);

        assert(strcmp(var, "t") == 0);

        assert(var_write_size[0] == 6);
        assert(var_write_size[1] == 7);
        assert(var_write_size[2] == 1);
        assert(var_write_size[3] == 0);

        prepare_input(test_case_2);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\nERROR\n\nOK\n\nOK\n\nOK\n") == 0);
        assert(strcmp(write_results, " CMD:\"\" CMD:\"\\\\\\\\\" CMD:\"r1\\nr2\\n\"") == 0);

        assert(strcmp(var, "r1\nr2\n") == 0);

        assert(var_write_size[0] == 0);
        assert(var_write_size[1] == 2);
        assert(var_write_size[2] == 6);
        assert(var_write_size[3] == 0);

        return 0;
}
