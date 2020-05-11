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

static int ap_read(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size)
{
        return 0;
}

static int ap_write(const struct cat_command *cmd, const uint8_t *data, const size_t data_size, const size_t args_num)
{
        return 0;
}

static struct cat_command cmds[] = {
        {
                .name = "AP1",
                .write = ap_write,
                .only_test = true
        },
        {
                .name = "AP2",
                .read = ap_read,
                .only_test = false
        },
};

static char buf[128];

static struct cat_command_group cmd_desc[] = {
        {
                .cmd = cmds,
                .cmd_num = sizeof(cmds) / sizeof(cmds[0]),
        }
};

static struct cat_descriptor desc = {
        .cmd_group = cmd_desc,
        .cmd_group_num = sizeof(cmd_desc) / sizeof(cmd_desc[0]),

        .buf = buf,
        .buf_size = sizeof(buf)
};

static int write_char(char ch)
{
        return 1;
}

static int read_char(char *ch)
{
        return 1;
}

static struct cat_io_interface iface = {
        .read = read_char,
        .write = write_char
};

int main(int argc, char **argv)
{
	struct cat_object at;
        struct cat_command const *cmd;

	cat_init(&at, &desc, &iface, NULL);

        cmd = cat_search_command_by_name(&at, "AP1");
        assert(cmd == &cmds[0]);

        cmd = cat_search_command_by_name(&at, "AP2");
        assert(cmd == &cmds[1]);

        cmd = cat_search_command_by_name(&at, "AP3");
        assert(cmd == NULL);

	return 0;
}
