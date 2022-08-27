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

static int a_run(const struct cat_command *cmd)
{
        strcat(run_results, " A:");
        strcat(run_results, cmd->name);
        return 0;
}

static int ap_run(const struct cat_command *cmd)
{
        strcat(run_results, " AP:");
        strcat(run_results, cmd->name);
        return 0;
}

static int test_run(const struct cat_command *cmd)
{
        strcat(run_results, " +TEST:");
        strcat(run_results, cmd->name);
        return 0;
}

static struct cat_command cmds[] = {
        {
                .name = "A",
                .run = a_run,
                .disable = false,
        },
        {
                .name = "AP",
                .run = ap_run,
                .disable = false,
        },
        {
                .name = "+TEST",
                .run = test_run,
                .disable = false,
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

static const char test_case_1[] = "\nsa\rAT\n\r\nAT\nAT+\n\nATA\r\natap\naaaattttap\na\n\r+test\r\n+testATA\nATAPATAP\n\rAT\rATA\nAT+test\r\n";

int main(int argc, char **argv)
{
        struct cat_object at;

        cat_init(&at, &desc, &iface, NULL);

        prepare_input(test_case_1);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\r\nERROR\r\n\nOK\n\nOK\n\r\nOK\r\n\nOK\n\nERROR\n\nERROR\n\r\nERROR\r\n\nERROR\n\nERROR\n\r\nERROR\r\n\r\nOK\r\n") == 0);
        assert(strcmp(run_results, " +TEST:+TEST A:A AP:AP +TEST:+TEST") == 0);

        prepare_input("\nAT\n");
        while (cat_service(&at) != 0) {};

        assert(cat_is_busy(&at) == 0);
        assert(strcmp(ack_results, "\nOK\n") == 0);
        assert(strcmp(run_results, "") == 0);

        prepare_input("\nAT+te");
        while (cat_service(&at) != 0) {};

        assert(cat_is_busy(&at) != 0);
        assert(strcmp(ack_results, "") == 0);
        assert(strcmp(run_results, "") == 0);

        prepare_input("st\n");
        while (cat_service(&at) != 0) {};

        assert(cat_is_busy(&at) == 0);
        assert(strcmp(ack_results, "\nOK\n") == 0);
        assert(strcmp(run_results, " +TEST:+TEST") == 0);

        struct cat_command *cmd;

        cmd = (struct cat_command*)cat_search_command_by_name(&at, "A");
        cmd->disable = true;
        cmd = (struct cat_command*)cat_search_command_by_name(&at, "+TEST");
        cmd->disable = true;

        prepare_input("\nATA\n\nATAP\n\nAT+TEST\n");
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\nOK\n\nOK\n\nERROR\n") == 0);
        assert(strcmp(run_results, " AP:AP AP:AP") == 0);

        struct cat_command_group *cmd_group;
        cmd_group = (struct cat_command_group*)cat_search_command_group_by_name(&at, "standard");
        assert(cmd_group == NULL);

        cmd_desc[0]->name = "standard";
        cmd_group = (struct cat_command_group*)cat_search_command_group_by_name(&at, "standard");
        assert(cmd_group == cmd_desc[0]);
        cmd_group->disable = true;

        prepare_input("\nATA\n\nATAP\n\nAT+TEST\n");
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\nERROR\n\nERROR\n\nERROR\n") == 0);
        assert(strcmp(run_results, "") == 0);

        return 0;
}
