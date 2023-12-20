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

static bool quit_flag;


struct spi_cmd_t {
	uint8_t address;
	uint16_t data;
	uint16_t mask;
};

struct spi_cmd_t spi_cmd; 


static int spi_write(const struct cat_command *cmd) {
	printf("WRITE SPI\n");
	printf("Address=0x%02hx, Value=%d(0x%04hx) with mask=%04hx\n", spi_cmd.address, (spi_cmd.data&spi_cmd.mask), (spi_cmd.data&spi_cmd.mask), spi_cmd.mask);
	return 0;
}

static int spi_read(const struct cat_command *cmd) {
        printf("READ SPI\n");
        printf("Address=0x%02hx, Value=%d(0x%04hx)\n", spi_cmd.address, spi_cmd.data, spi_cmd.data);
        return 0;
}

static int SpiResetMask(const struct cat_variable *var, size_t write_size) {
	spi_cmd.mask = 0xffff;
	return 0;
}

static int quit_run(const struct cat_command *cmd)
{
        printf("QUIT: <%s>", cmd->name);
        quit_flag = true;
        return 0;
}

static int print_cmd_list(const struct cat_command *cmd)
{
        return CAT_RETURN_STATE_PRINT_CMD_LIST_OK;
}

static struct cat_variable data_spi_write[] = {
        {
                .type = CAT_VAR_UINT_HEX,
                .data = &(spi_cmd.address),
                .data_size = sizeof(spi_cmd.address),
                .name = "address",
                .access = CAT_VAR_ACCESS_READ_WRITE
        }, {
                .type = CAT_VAR_UINT_HEX,
                .data = &(spi_cmd.data),
                .data_size = sizeof(spi_cmd.data),
                .write = SpiResetMask,
                .name = "data",
                .access = CAT_VAR_ACCESS_READ_WRITE
        }, {
                .type = CAT_VAR_UINT_HEX,
                .data = &(spi_cmd.mask),
                .data_size = sizeof(spi_cmd.mask),
                .name = "mask",
                .access = CAT_VAR_ACCESS_READ_WRITE
        }
};

static struct cat_variable data_spi_read[] = {
        {
                .type = CAT_VAR_UINT_HEX,
                .data = &(spi_cmd.address),
                .data_size = sizeof(spi_cmd.address),
                .name = "address",
                .access = CAT_VAR_ACCESS_READ_WRITE
        }
};

static struct cat_command cmds[] = {
        {
        .name           = "+SPIW",
        //.description    = "Write to SPI",
        .var            = data_spi_write,
        .var_num        = sizeof(data_spi_write) / sizeof(data_spi_write[0]),
        .need_all_vars  = false,
        .write            = spi_write
        },
        {
        .name           = "+SPIR",
        .description    = "Read from SPI",
        .var            = data_spi_read,
        .var_num        = sizeof(data_spi_read) / sizeof(data_spi_read[0]),
        .need_all_vars = true,
        .write           = spi_read
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
        putc(ch, stdout);
        return 1;
}

static int read_char(char *ch)
{
        *ch = getc(stdin);
        return 1;
}

static struct cat_io_interface iface = {
        .read = read_char,
        .write = write_char
};

int main(int argc, char **argv)
{
        struct cat_object at;

        cat_init(&at, &desc, &iface, NULL);

        while ((cat_service(&at) != 0) && (quit_flag == 0)) {};

        printf("Bye!\n");

        return 0;
}
