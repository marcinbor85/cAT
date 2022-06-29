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

static int e_run(const struct cat_command *cmd)
{
        strcat(run_results, " E:");
        strcat(run_results, cmd->name);
        return 0;
}

static int e0_run(const struct cat_command *cmd)
{
        strcat(run_results, " E0:");
        strcat(run_results, cmd->name);
        return 0;
}

static int e1_run(const struct cat_command *cmd)
{
        strcat(run_results, " E1:");
        strcat(run_results, cmd->name);
        return 0;
}

static struct cat_command cmds1[] = {
        {
                .name = "E",
                .run = e_run,
        },
        {
                .name = "E0",
                .run = e0_run,
        },
        {
                .name = "E1",
                .run = e1_run,
        }
};

static struct cat_command cmds2[] = {
        {
                .name = "E0",
                .run = e0_run,
        },
        {
                .name = "E1",
                .run = e1_run,
        },
        {
                .name = "E",
                .run = e_run,
        }
};

static struct cat_command cmds3[] = {
        {
                .name = "E0",
                .run = e0_run,
        },
        {
                .name = "E",
                .run = e_run,
        },
        {
                .name = "E1",
                .run = e1_run,
        }
};

static char buf[128];

static struct cat_command_group cmd_1_group = {
        .cmd = cmds1,
        .cmd_num = sizeof(cmds1) / sizeof(cmds1[0]),
};

static struct cat_command_group *cmd_1_desc[] = {
        &cmd_1_group
};

static struct cat_descriptor desc_1 = {
        .cmd_group = cmd_1_desc,
        .cmd_group_num = sizeof(cmd_1_desc) / sizeof(cmd_1_desc[0]),

        .buf = buf,
        .buf_size = sizeof(buf),
};

static struct cat_command_group cmd_2_group = {
        .cmd = cmds2,
        .cmd_num = sizeof(cmds2) / sizeof(cmds2[0]),
};

static struct cat_command_group *cmd_2_desc[] = {
        &cmd_2_group
};

static struct cat_descriptor desc_2 = {
        .cmd_group = cmd_2_desc,
        .cmd_group_num = sizeof(cmd_2_desc) / sizeof(cmd_2_desc[0]),

        .buf = buf,
        .buf_size = sizeof(buf),
};

static struct cat_command_group cmd_3_group = {
        .cmd = cmds3,
        .cmd_num = sizeof(cmds3) / sizeof(cmds3[0]),
};

static struct cat_command_group *cmd_3_desc[] = {
        &cmd_3_group
};

static struct cat_descriptor desc_3 = {
        .cmd_group = cmd_3_desc,
        .cmd_group_num = sizeof(cmd_3_desc) / sizeof(cmd_3_desc[0]),

        .buf = buf,
        .buf_size = sizeof(buf),
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

static const char test_case_1[] = "\nATE\n\nATE0\n\nATE1\n";

int main(int argc, char **argv)
{
        struct cat_object at;

        cat_init(&at, &desc_1, &iface, NULL);

        prepare_input(test_case_1);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\nOK\n\nOK\n\nOK\n") == 0);
        assert(strcmp(run_results, " E:E E0:E0 E1:E1") == 0);

        cat_init(&at, &desc_2, &iface, NULL);

        prepare_input(test_case_1);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\nOK\n\nOK\n\nOK\n") == 0);
        assert(strcmp(run_results, " E:E E0:E0 E1:E1") == 0);

        cat_init(&at, &desc_3, &iface, NULL);

        prepare_input(test_case_1);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\nOK\n\nOK\n\nOK\n") == 0);
        assert(strcmp(run_results, " E:E E0:E0 E1:E1") == 0);

        return 0;
}
