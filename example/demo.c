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
	unsigned char data_set_with_address;
};

struct spi_cmd_t spi_cmd; 


static int spi_write(const struct cat_command *cmd, const uint8_t *data, const size_t data_size, const size_t args_num) {
	if(spi_cmd.data_set_with_address == 0) {
		printf("Set data before write\n");
		return -1;
	}
	printf("WRITE SPI\n");
	printf("Address=0x%02hx, Value=%d(0x%04hx)\n", spi_cmd.address, spi_cmd.data, spi_cmd.data);
	return 0;
}

static int spi_read(const struct cat_command *var, uint8_t *data, size_t *data_size, const size_t max_data_size) {
        printf("READ SPI\n");
		printf("Address=0x%02hx, Value=%d(0x%04hx)\n", spi_cmd.address, spi_cmd.data, spi_cmd.data);
        return 0;
}

static int SpiResetIndex(const struct cat_command *cmd, const uint8_t *data, const size_t data_size, const size_t args_num) {
	spi_cmd.data_set_with_address = 0;
	return 0;
}

static int SpiSetIndex(const struct cat_command *cmd, const uint8_t *data, const size_t data_size, const size_t args_num) {
	spi_cmd.data_set_with_address = 1;
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

static struct cat_variable data_spi[] = {
        {
                .type = CAT_VAR_UINT_HEX,
                .data = &(spi_cmd.address),
                .data_size = sizeof(spi_cmd.address),
                .write = SpiResetIndex,
                .name = "address",
                .access = CAT_VAR_ACCESS_READ_WRITE
        }, {
                .type = CAT_VAR_UINT_HEX,
                .data = &(spi_cmd.data),
                .data_size = sizeof(spi_cmd.data),
                .write = SpiSetIndex,
                .name = "data",
                .access = CAT_VAR_ACCESS_READ_WRITE
        }
};

static struct cat_command cmds[] = {
		{
                .name           = "+SPI",
                .description    = "Fill SPI buffer",
                .var            = data_spi,
                .var_num        = sizeof(data_spi) / sizeof(data_spi[0]),
                .need_all_vars = false
		},
		{
                .name           = "+SPIR",
                .description    = "Read SPI from buffer",
                .run           = spi_read
		},
		{
                .name           = "+SPIW",
                .description    = "Write SPI from buffer",
                .run            = spi_write
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
