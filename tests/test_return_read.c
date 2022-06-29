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

static char cmd_results[256];
static char ack_results[256];

static char const *input_text;
static size_t input_index;

static int var_x;
static struct cat_object at;

static cat_return_state ret;
static bool ret_error;

static cat_return_state cmd_read(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size)
{
        strcat(cmd_results, " read:");
        strcat(cmd_results, cmd->name);

        if (cat_is_hold(&at) == CAT_STATUS_HOLD) {
                var_x++;
                if (var_x > 4) {
                        ret = (ret_error == false) ? CAT_RETURN_STATE_HOLD_EXIT_OK : CAT_RETURN_STATE_HOLD_EXIT_ERROR;
                } else {
                        if (var_x == 4) {
                                strcpy(data, "test");
                                *data_size = strlen(data);
                        }
                        ret = CAT_RETURN_STATE_DATA_NEXT;
                }
        } else {
                if ((ret == CAT_RETURN_STATE_DATA_NEXT) || (ret == CAT_RETURN_STATE_NEXT)) {
                        var_x++;
                        if (var_x > 2)
                                ret = CAT_RETURN_STATE_DATA_OK;
                } else if (ret == CAT_RETURN_STATE_HOLD) {
                        cat_trigger_unsolicited_read(&at, cmd);
                }
        }

        return ret;
}

static struct cat_variable vars[] = {
        {
                .name = "X",
                .type = CAT_VAR_INT_DEC,
                .data = &var_x,
                .data_size = sizeof(var_x)
        }
};

static struct cat_command cmds[] = {
        {
                .name = "+CMD",
                .read = cmd_read,
                .var = vars,
                .var_num = sizeof(vars) / sizeof(vars[0]),
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
        var_x = 1;

        memset(ack_results, 0, sizeof(ack_results));
        memset(cmd_results, 0, sizeof(cmd_results));
}

static const char test_case_1[] = "\nAT+CMD?\n";

int main(int argc, char **argv)
{
        cat_status s;

        cat_init(&at, &desc, &iface, NULL);

        ret = CAT_RETURN_STATE_ERROR;
        prepare_input(test_case_1);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\nERROR\n") == 0);
        assert(strcmp(cmd_results, " read:+CMD") == 0);

        ret = CAT_RETURN_STATE_DATA_OK;
        prepare_input(test_case_1);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\n+CMD=1\n\nOK\n") == 0);
        assert(strcmp(cmd_results, " read:+CMD") == 0);

        ret = CAT_RETURN_STATE_DATA_NEXT;
        prepare_input(test_case_1);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\n+CMD=1\n\n+CMD=2\n\nOK\n") == 0);
        assert(strcmp(cmd_results, " read:+CMD read:+CMD") == 0);

        ret = CAT_RETURN_STATE_NEXT;
        prepare_input(test_case_1);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\n+CMD=2\n\nOK\n") == 0);
        assert(strcmp(cmd_results, " read:+CMD read:+CMD") == 0);

        ret = CAT_RETURN_STATE_OK;
        prepare_input(test_case_1);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\nOK\n") == 0);
        assert(strcmp(cmd_results, " read:+CMD") == 0);

        ret_error = false;
        ret = CAT_RETURN_STATE_HOLD;
        prepare_input(test_case_1);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\n+CMD=1\n\n+CMD=2\n\ntest\n\nOK\n") == 0);
        assert(strcmp(cmd_results, " read:+CMD read:+CMD read:+CMD read:+CMD read:+CMD") == 0);

        ret_error = true;
        ret = CAT_RETURN_STATE_HOLD;
        prepare_input(test_case_1);
        while (cat_service(&at) != 0) {};

        assert(strcmp(ack_results, "\n+CMD=1\n\n+CMD=2\n\ntest\n\nERROR\n") == 0);
        assert(strcmp(cmd_results, " read:+CMD read:+CMD read:+CMD read:+CMD read:+CMD") == 0);

        return 0;
}
