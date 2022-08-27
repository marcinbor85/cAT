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

/* run command handler with application dependent login print code */
static int print_run(const struct cat_command *cmd)
{
        printf("some printing at (%d,%d) with text \"%s\"\n", x, y, message);
        return 0;
}

/* run command handler attached to HELP command for printing commands list */
static int print_cmd_list(const struct cat_command *cmd)
{
        return CAT_RETURN_STATE_PRINT_CMD_LIST_OK;
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
                .name = "X",
                .access = CAT_VAR_ACCESS_READ_WRITE,
        },
        {
                .type = CAT_VAR_UINT_DEC,
                .data = &y,
                .data_size = sizeof(y),
                .name = "Y",
                .access = CAT_VAR_ACCESS_READ_WRITE,
        },
        {
                .type = CAT_VAR_BUF_STRING,
                .data = message,
                .data_size = sizeof(message),
                .name = "MESSAGE",
                .access = CAT_VAR_ACCESS_READ_WRITE,
        }
};

/* declaring commands array */
static struct cat_command cmds[] = {
        {
                .name = "+PRINT",
                .description = "Printing something special at (X,Y).",
                .run = print_run,
                .var = print_vars,
                .var_num = sizeof(print_vars) / sizeof(print_vars[0]),
                .need_all_vars = true
        },
        {
                .name = "#HELP",
                .run = print_cmd_list,
        },
        {
                .name = "#QUIT",
                .run = quit_run
        },
};

/* working buffer */
static char buf[128];

/* declaring parser descriptor */
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
        struct cat_object at;

        /* initializing */
        cat_init(&at, &desc, &iface, NULL);

        /* main loop with exit code conditions */
        while ((cat_service(&at) != 0) && (quit_flag == 0)) {};

        /* goodbye message */
        printf("Bye!\n");

        return 0;
}
