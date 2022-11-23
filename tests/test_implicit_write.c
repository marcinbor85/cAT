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

static char run_results[256];
static char ack_results[256];

static char const *input_text;
static size_t input_index;

static cat_return_state cmd_write(const struct cat_command *cmd, const uint8_t *data, const size_t data_size, const size_t args_num)
{
        strcat(run_results, " W_");
        strcat(run_results, cmd->name);
        strcat(run_results, ":");
        strncat(run_results, data, data_size);

        return CAT_RETURN_STATE_OK;
}

static cat_return_state cmd_read(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size)
{
        strcat(run_results, " D_");
        strcat(run_results, cmd->name);
        return CAT_RETURN_STATE_OK;
}

static cat_return_state cmd_run(const struct cat_command *cmd)
{
        strcat(run_results, " R_");
        strcat(run_results, cmd->name);
        return CAT_RETURN_STATE_OK;
}

static cat_return_state cmd_test(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size)
{
        strcat(run_results, " T_");
        strcat(run_results, cmd->name);
        return CAT_RETURN_STATE_OK;
}

static int8_t var1;
static int8_t var2;

static struct cat_variable vars[] = {
        {
                .type = CAT_VAR_INT_DEC,
                .data = &var1,
                .data_size = sizeof(var1),
                .access = CAT_VAR_ACCESS_READ_WRITE
        },
        {
                .type = CAT_VAR_INT_DEC,
                .data = &var2,
                .data_size = sizeof(var2),
                .access = CAT_VAR_ACCESS_READ_WRITE
        }
};

static struct cat_command cmds[] = {
        {
                .name = "DTEST", /* Won't ever be executed, since prefix matches implicit write command */
                .write = cmd_write,
                .read = cmd_read,
                .run = cmd_run,
                .test = cmd_test
        },
        {
                .name = "D",
                .write = cmd_write,
                .read = NULL,
                .run = NULL,
                .test = NULL,
                .implicit_write = true
        },
        {
                .name = "+D",
                .write = cmd_write,
                .read = cmd_read,
                .run = cmd_run,
                .test = cmd_test
        },
        {
                .name = "V",
                .write = cmd_write,
                .read = NULL,
                .run = NULL,
                .test = NULL,
                .var = vars,
                .var_num = sizeof(vars) / sizeof(vars[0]),
                .implicit_write = true
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

        memset(run_results, 0, sizeof(run_results));
        memset(ack_results, 0, sizeof(ack_results));
}

static const char test_case_1[] = "\nATDTEST\nATD\nATD0\nATD1\nATD?\nATD=1\nATD=?\nAT+D\nAT+D?\nAT+D=0\nAT+D=?\nATV-1,42\n";

int main(int argc, char **argv)
{
        struct cat_object at;

        cat_init(&at, &desc, &iface, NULL);

        prepare_input(test_case_1);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\nOK\n\nOK\n\nOK\n\nOK\n\nOK\n\nOK\n\nOK\n\nOK\n\nOK\n\nOK\n\nOK\n\nOK\n") == 0);
        assert(strcmp(run_results, " W_D:TEST W_D: W_D:0 W_D:1 W_D:? W_D:=1 W_D:=? R_+D D_+D W_+D:0 T_+D W_V:-1,42") == 0);
        assert(var1 == -1);
        assert(var2 == 42);

        return 0;
}
