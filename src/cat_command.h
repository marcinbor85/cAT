/*
MIT License

Copyright (c) 2020 Marcin Borowicz

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

#ifndef CAT_COMMAND_H
#define CAT_COMMAND_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "cat_common.h"
#include "cat_variable.h"

/* only forward declarations (looks for definition below) */
struct cat_command;

/**
 * Write command function handler (AT+CMD=)
 * 
 * This callback function is called after parsing all connected variables.
 * User application can validate all variales at once at this moment, or copy them to the other application layer buffer.
 * User can check number of variables or make custom process of parsing non standard arguments format.
 * This handler is optional, so when is not defined, operations on variables will be fully automatically.
 * If neither write handler nor variables not defined, then write command type is not available.
 * 
 * @param cmd - pointer to struct descriptor of processed command
 * @param data - pointer to arguments buffer for custom parsing
 * @param data_size - length of arguments buffer
 * @param args_num - number of passed arguments connected to variables
 * @return according to cat_return_state enum definitions
 * */
typedef cat_return_state (*cat_cmd_write_handler)(const struct cat_command *cmd, const uint8_t *data, const size_t data_size, const size_t args_num);

/**
 * Read command function handler (AT+CMD?)
 * 
 * This callback function is called after format all connected variables.
 * User application can change automatic response, or add some custom data to response.
 * This handler is optional, so when is not defined, operations on variables will be fully automatically.
 * If neither read handler nor variables not defined, then read command type is not available.
 * 
 * @param cmd - pointer to struct descriptor of processed command
 * @param data - pointer to arguments buffer for custom parsing
 * @param data_size - pointer to length of arguments buffer (can be modifed if needed)
 * @param max_data_size - maximum length of buffer pointed by data pointer
 * @return according to cat_return_state enum definitions
 * */
typedef cat_return_state (*cat_cmd_read_handler)(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size);

/**
 * Run command function handler (AT+CMD)
 * 
 * No operations on variables are done.
 * This handler is optional.
 * If run handler not defined, then run command type is not available.
 * 
 * @param cmd - pointer to struct descriptor of processed command
 * @return according to cat_return_state enum definitions
 * */
typedef cat_return_state (*cat_cmd_run_handler)(const struct cat_command *cmd);

/**
 * Test command function handler (AT+CMD=?)
 * 
 * This callback function is called after format all connected variables.
 * User application can change automatic response, or add some custom data to response.
 * This handler is optional, so when is not defined, operations on variables will be fully automatically.
 * If neither test handler nor variables not defined, then test command type is not available.
 * Exception of this rule is write command without variables.
 * In this case, the "question mark" will be passed as a custom argument to the write handler.
 * 
 * @param cmd - pointer to struct descriptor of processed command
 * @param data - pointer to arguments buffer for custom parsing
 * @param data_size - pointer to length of arguments buffer (can be modifed if needed)
 * @param max_data_size - maximum length of buffer pointed by data pointer
 * @return according to cat_return_state enum definitions
 * */
typedef cat_return_state (*cat_cmd_test_handler)(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size);

/* structure with at command descriptor */
struct cat_command {
        const char *name; /* at command name (case-insensitivity) */
        const char *description; /* at command description (optionally - can be null) */

        cat_cmd_write_handler write; /* write command handler */
        cat_cmd_read_handler read; /* read command handler */
        cat_cmd_run_handler run; /* run command handler */
        cat_cmd_test_handler test; /* test command handler */

        struct cat_variable const *var; /* pointer to array of variables assiocated with this command */
        size_t var_num; /* number of variables in array */
        bool need_all_vars; /* flag to need all vars parsing */
        bool only_test; /* flag to disable read/write/run commands (only test auto description) */
};

#ifdef __cplusplus
}
#endif

#endif /* CAT_COMMAND_H */
