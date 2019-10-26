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

/* write command function handler */
typedef int (*cat_cmd_write_handler)(const char *name, const uint8_t *data, const size_t data_size);
/* read command function handler */
typedef int (*cat_cmd_read_handler)(const char *name, uint8_t *data, size_t *data_size, const size_t max_data_size);
/* run command function handler */
typedef int (*cat_cmd_run_handler)(const char *name);

/* enum type with main at parser fsm state */
typedef enum {
        CAT_STATE_ERROR = -1,
        CAT_STATE_PARSE_PREFIX,
        CAT_STATE_PARSE_COMMAND_CHAR,
        CAT_STATE_UPDATE_COMMAND_STATE,
        CAT_STATE_WAIT_ACKNOWLEDGE,
        CAT_STATE_SEARCH_COMMAND,
        CAT_STATE_COMMAND_FOUND,
        CAT_STATE_COMMAND_NOT_FOUND,
        CAT_STATE_PARSE_COMMAND_ARGS
} cat_state;

/* enum type with prefix parser fsm state */
typedef enum {
        CAT_PREFIX_STATE_WAIT_A = 0,
        CAT_PREFIX_STATE_WAIT_T,
} cat_prefix_state;

/* enum type with type of command request */
typedef enum {
        CAT_CMD_TYPE_EXECUTE = 0,
        CAT_CMD_TYPE_READ,
        CAT_CMD_TYPE_WRITE
} cat_cmd_type;

/* structure with io interface functions */
struct cat_io_interface {
	int (*write)(char ch); /* write char to output stream. return 1 if byte wrote successfully. */
	int (*read)(char *ch); /* read char from input stream. return 1 if byte read successfully. */
};

/* structure with at command descriptor */
struct cat_command {
	const char *name; /* at command name (case-insensitivity) */

	cat_cmd_write_handler write; /* write command handler */
	cat_cmd_read_handler read; /* read command handler */
	cat_cmd_run_handler run; /* run command handler */
};

/* structure with at command parser descriptor */
struct cat_descriptor {
	struct cat_command const *cmd; /* pointer to array of commands descriptor */
	size_t cmd_num; /* number of commands in array */

        uint8_t *buf; /* pointer to working buffer (used to parse command argument and command states) */
        size_t buf_size; /* working buffer length */
};

/* structure with main at command parser object */
struct cat_object {
	struct cat_descriptor const *desc; /* pointer to at command parser descriptor */
	struct cat_io_interface const *iface; /* pointer to at command parser io interface */

        size_t index; /* index used to iterate over commands */
        size_t length; /* length of input command name and command arguments */

        struct cat_command const *cmd; /* pointer to current command descriptor */
        cat_cmd_type cmd_type; /* type of command request */

        char current_char; /* current received char from input stream */
        cat_state state; /* current fsm state */
        cat_prefix_state prefix_state; /* current prefix fsm state */
};

/**
 * Function used to initialize at command parser.
 * Initialize starting values of object fields.
 * 
 * @param self pointer to at command parser object to initialize
 * @param desc pointer to at command parser descriptor
 * @param iface pointer to at command parser io low-level layer interface
 */
void cat_init(struct cat_object *self, const struct cat_descriptor *desc, const struct cat_io_interface *iface);

/**
 * Function must be called periodically to asynchronoulsy run at command parser.
 * Commands handlers will be call from this function context.
 * 
 * @param self pointer to at command parser object to initialize
 * @return 0 - nothing to do, waiting for input char, 1 - busy, parsing in progress.
 */
int cat_service(struct cat_object *self);

#ifdef __cplusplus
}
#endif

#endif /* CAT_H */