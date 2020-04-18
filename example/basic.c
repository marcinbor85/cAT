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

/* variables assigned to print command */
static uint8_t x;
static uint8_t y;
static char message[16];

/* helper variable used to exit demo code */
static bool quit_flag;

/* main global cat_object parser object structure */
static struct cat_object at;

/* variable assigned to unsolicited command */
static uint8_t idx;

/* declaring unsolicited command variables array */
static struct cat_variable unsolicited_read_cmd_vars[] = {
        {
                .type = CAT_VAR_UINT_DEC,
                .data = &idx,
                .data_size = sizeof(idx),
        }
};

static cat_return_state unsolicited_read(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size);

/* declaring unsolicited command */
static struct cat_command unsolicited_read_cmd = {
        .name = "+EVENT",
        .read = unsolicited_read,
        .var = unsolicited_read_cmd_vars,
        .var_num = sizeof(unsolicited_read_cmd_vars) / sizeof(unsolicited_read_cmd_vars[0]),
};

/* unsolicited cmd read handler */
static cat_return_state unsolicited_read(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size)
{
        idx++;

        if (idx > 3) {
                cat_hold_exit(&at, CAT_STATUS_OK);
        } else {
                cat_trigger_unsolicited_read(&at, &unsolicited_read_cmd);
        }

        return CAT_STATUS_OK;
}

/* write command handler with application dependent login print code */
static int print_write(const struct cat_command *cmd, const uint8_t *data, const size_t data_size, const size_t args_num)
{
        idx = 1;

        /* this function can be invoked asynchronous to cat_service, but in this simple example is called here */
        cat_trigger_unsolicited_read(&at, &unsolicited_read_cmd);

        return CAT_RETURN_STATE_HOLD;
}

/* run command handler with application dependent login print code */
static int print_run(const struct cat_command *cmd)
{
        printf("some printing at (%d,%d) with text \"%s\"\n", x, y, message);
        return 0;
}

/* run command handler with custom exit mechanism */
static int quit_run(const struct cat_command *cmd)
{
        quit_flag = true;
        return 0;
}

/* declaring print variables array */
static struct cat_variable print_vars[] = {
        {
                .type = CAT_VAR_UINT_DEC,
                .data = &x,
                .data_size = sizeof(x),
                .name = "X"
        },
        {
                .type = CAT_VAR_UINT_DEC,
                .data = &y,
                .data_size = sizeof(y),
                .name = "Y"
        },
        {
                .type = CAT_VAR_BUF_STRING,
                .data = message,
                .data_size = sizeof(message),
                .name = "MESSAGE"
        }
};

/* declaring commands array */
static struct cat_command cmds[] = {
        {
                .name = "+PRINT",
                .description = "Printing something special at (X,Y).",
                .run = print_run,
                .write = print_write,
                .var = print_vars,
                .var_num = sizeof(print_vars) / sizeof(print_vars[0]),
                .need_all_vars = true
        },
        {
                .name = "#QUIT",
                .run = quit_run
        },
};

/* working buffer */
static char buf[128];

/* declaring parser descriptor */
static struct cat_descriptor desc = {
        .cmd = cmds,
        .cmd_num = sizeof(cmds) / sizeof(cmds[0]),

        .buf = buf,
        .buf_size = sizeof(buf)
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
