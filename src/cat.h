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

#ifndef CAT_H
#define CAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

typedef int (*cat_cmd_write_handler)(const char *name, const uint8_t *data, const size_t data_size);
typedef int (*cat_cmd_read_handler)(const char *name, uint8_t *data, size_t *data_size, const size_t max_data_size);
typedef int (*cat_cmd_run_handler)(const char *name);

typedef enum {
        CAT_STATE_ERROR = -1,
        CAT_STATE_PARSE_PREFIX,
        CAT_STATE_PARSE_COMMAND_CHAR,
        CAT_STATE_UPDATE_COMMAND_STATE,
        CAT_STATE_WAIT_ACKNOWLEDGE,
        CAT_STATE_SEARCH_COMMAND,
        CAT_STATE_COMMAND_FOUND,
        CAT_STATE_COMMAND_NOT_FOUND
} cat_state;

typedef enum {
        CAT_PREFIX_STATE_WAIT_A = 0,
        CAT_PREFIX_STATE_WAIT_T,
} cat_prefix_state;

typedef enum {
        CAT_CMD_TYPE_EXECUTE = 0,
        CAT_CMD_TYPE_READ,
        CAT_CMD_TYPE_WRITE
} cat_cmd_type;

struct cat_io_interface {
	int (*write)(char ch);
	int (*read)(char *ch);
};

struct cat_command {
	const char *name;

	cat_cmd_write_handler write;
	cat_cmd_read_handler read;
	cat_cmd_run_handler run;
};

struct cat_descriptor {
	struct cat_command const *cmd;
	size_t cmd_num;

        uint8_t *buf;
        size_t buf_size;
};

struct cat_object {
	struct cat_descriptor const *desc;
	struct cat_io_interface const *iface;

        size_t index;
        size_t length;

        size_t cmd_index;
        cat_cmd_type cmd_type;

        char current_char;
        cat_state state;
        cat_prefix_state prefix_state;
};

void cat_init(struct cat_object *self, const struct cat_descriptor *desc, const struct cat_io_interface *iface);
int cat_service(struct cat_object *self);

#ifdef __cplusplus
}
#endif

#endif /* CAT_H */