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

static char test_results[256];
static char ack_results[256];

static char const *input_text;
static size_t input_index;

static int a_test(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size)
{
        strcat(test_results, " A:");
        strcat(test_results, cmd->name);

        snprintf(data, max_data_size, "%s=A-val", cmd->name);
        *data_size = strlen(data);

        return 0;
}

static int ap_test(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size)
{
        strcat(test_results, " AP:");
        strcat(test_results, cmd->name);

        *data_size = 0;

        return 0;
}

static int ap_write(const struct cat_command *cmd, const uint8_t *data, const size_t data_size, const size_t args_num)
{
        strcat(test_results, " AP_W:");
        strcat(test_results, cmd->name);

        assert(args_num == 0);
        assert(data[0] == 'a');
        assert(data_size == 1);

        return 0;
}

static int apw_write(const struct cat_command *cmd, const uint8_t *data, const size_t data_size, const size_t args_num)
{
        strcat(test_results, " APW:");
        strcat(test_results, cmd->name);

        assert(args_num == 0);
        assert(data[0] == '?');
        assert(data_size == 1);

        return 0;
}

static int test_test(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size)
{
        strcat(test_results, " +TEST:");
        strcat(test_results, cmd->name);

        return -1;
}

static struct cat_command cmds[] = {
        {
                .name = "A",
                .test = a_test
        },
        {
                .name = "AP",
                .test = ap_test,
                .write = ap_write
        },
        {
                .name = "APW",
                .write = apw_write
        },
        {
                .name = "+TEST",
                .test = test_test
        },
        {
                .name = "+EMPTY"
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

        memset(ack_results, 0, sizeof(ack_results));
        memset(test_results, 0, sizeof(test_results));
}

static const char test_case_1[] = "\nAT\r\nAT\nATAP=?\nATAP=?a\nATAP=a\nATAPW=?\nAT+TEST=?\nATA=?\nAT+EMPTY=?\n";

int main(int argc, char **argv)
{
        struct cat_object at;

        cat_init(&at, &desc, &iface, NULL);

        prepare_input(test_case_1);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\r\nOK\r\n\nOK\n\nAP=\n\nOK\n\nERROR\n\nOK\n\nOK\n\nERROR\n\nA=A-val\n\nOK\n\nERROR\n") == 0);
        assert(strcmp(test_results, " AP:AP AP_W:AP APW:APW +TEST:+TEST A:A") == 0);

        return 0;
}
