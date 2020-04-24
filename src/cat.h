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

#include "cat_common.h"
#include "cat_variable.h"
#include "cat_command.h"

/* enum type with main at parser fsm state */
typedef enum {
        CAT_STATE_ERROR = -1,
        CAT_STATE_IDLE,
        CAT_STATE_PARSE_PREFIX,
        CAT_STATE_PARSE_COMMAND_CHAR,
        CAT_STATE_UPDATE_COMMAND_STATE,
        CAT_STATE_WAIT_READ_ACKNOWLEDGE,
        CAT_STATE_SEARCH_COMMAND,
        CAT_STATE_COMMAND_FOUND,
        CAT_STATE_COMMAND_NOT_FOUND,
        CAT_STATE_PARSE_COMMAND_ARGS,
        CAT_STATE_PARSE_WRITE_ARGS,
        CAT_STATE_FORMAT_READ_ARGS,
        CAT_STATE_WAIT_TEST_ACKNOWLEDGE,
        CAT_STATE_FORMAT_TEST_ARGS,
        CAT_STATE_WRITE_LOOP,
        CAT_STATE_READ_LOOP,
        CAT_STATE_TEST_LOOP,
        CAT_STATE_RUN_LOOP,
        CAT_STATE_HOLD,
        CAT_STATE_FLUSH_IO_WRITE,
        CAT_STATE_AFTER_FLUSH_RESET,
        CAT_STATE_AFTER_FLUSH_OK,
        CAT_STATE_AFTER_FLUSH_FORMAT_READ_ARGS,
        CAT_STATE_AFTER_FLUSH_FORMAT_TEST_ARGS,
} cat_state;

/* structure with at command parser descriptor */
struct cat_descriptor {
        struct cat_command const *cmd; /* pointer to array of commands descriptor */
        size_t cmd_num; /* number of commands in array */

        uint8_t *buf; /* pointer to working buffer (used to parse command argument) */
        size_t buf_size; /* working buffer length */
};

/* structure with main at command parser object */
struct cat_object {
        struct cat_descriptor const *desc; /* pointer to at command parser descriptor */
        struct cat_io_interface const *io; /* pointer to at command parser io interface */
        struct cat_mutex_interface const *mutex; /* pointer to at command parser mutex interface */

        size_t index; /* index used to iterate over commands and variables */
        size_t length; /* length of input command name and command arguments */
        size_t position; /* position of actually parsed char in arguments string */
        size_t write_size; /* size of parsed buffer hex or buffer string */

        struct cat_command const *cmd; /* pointer to current command descriptor */
        struct cat_variable const *var; /* pointer to current variable descriptor */
        cat_cmd_type cmd_type; /* type of command request */

        char current_char; /* current received char from input stream */
        cat_state state; /* current fsm state */
        bool cr_flag; /* flag for detect <cr> char in input string */
        bool disable_ack; /* flag for disabling ACK messages OK/ERROR during unsolicited read */
        bool hold_state_flag; /* status of hold state (independent from fsm states) */
        int hold_exit_status; /* hold exit parameter with status */
        char const *write_buf; /* working buffer pointer used for asynch writing to io */
        int write_state; /* before, data, after flush io write state */
        cat_state write_state_after; /* parser state to set after flush io write */

        struct cat_command const *unsolicited_read_cmd; /* pointer to command used to unsolicited read */
        struct cat_command const *unsolicited_test_cmd; /* pointer to command used to unsolicited test */
};

/**
 * Function used to initialize at command parser.
 * Initialize starting values of object fields.
 * 
 * @param self pointer to at command parser object to initialize
 * @param desc pointer to at command parser descriptor
 * @param io pointer to at command parser io low-level layer interface
 * @param mutex pointer to at command partes mutex interface
 */
void cat_init(struct cat_object *self, const struct cat_descriptor *desc, const struct cat_io_interface *io, const struct cat_mutex_interface *mutex);

/**
 * Function must be called periodically to asynchronoulsy run at command parser.
 * Commands handlers will be call from this function context.
 * 
 * @param self pointer to at command parser object
 * @return according to cat_return_state enum definitions
 */
cat_status cat_service(struct cat_object *self);

/**
 * Function return flag which indicating internal busy state.
 * It is used to determine whether external application modules can use shared input / output interfaces functions.
 * It is usefull especially in rtos environments.
 * If internal parser state is busy by doing some processing then function return 1.
 * If the function returns 0, then the external application modules can safely use the input / output interfaces functions shared with the library.
 * If the function returns 1, then input / output interface function are used by internal parser functions.
 * 
 * @param self pointer to at command parser object
 * @return according to cat_return_state enum definitions
 */
cat_status cat_is_busy(struct cat_object *self);

/**
 * Function return flag which indicating parsing hold state.
 * If the function returns 0, then the at parsing process is normal.
 * If the function returns 1, then the at parsing process is holded.
 * To exit from hold state, user have to call cat_hold_exit or return HOLD_EXIT return value in callback.
 * 
 * @param self pointer to at command parser object
 * @return according to cat_return_state enum definitions
 */
cat_status cat_is_hold(struct cat_object *self);

/**
 * Function sends unsolicited read event message.
 * Command message is buffered inside parser in 1-level deep buffer and processed in cat_service context.
 * Only command pointer is buffered, so command struct should be static or global until be fully processed.
 * 
 * @param self pointer to at command parser object
 * @param cmd pointer to command structure regarding which unsolicited read applies to
 * @return according to cat_return_state enum definitions
 */
cat_status cat_trigger_unsolicited_read(struct cat_object *self, struct cat_command const *cmd);

/**
 * Function sends unsolicited test event message.
 * Command message is buffered inside parser in 1-level deep buffer and processed in cat_service context.
 * Only command pointer is buffered, so command struct should be static or global until be fully processed.
 * 
 * @param self pointer to at command parser object
 * @param cmd pointer to command structure regarding which unsolicited test applies to
 * @return according to cat_return_state enum definitions
 */
cat_status cat_trigger_unsolicited_test(struct cat_object *self, struct cat_command const *cmd);

/**
 * Function used to exit from hold state with OK/ERROR response and back to idle state.
 * 
 * @param self pointer to at command parser object
 * @param status response status 0 - OK, else ERROR
 * @return according to cat_return_state enum definitions
 */
cat_status cat_hold_exit(struct cat_object *self, cat_status status);

/**
 * Function used to searching registered command by its name.
 * 
 * @param self pointer to at command parser object
 * @param name command name to search
 * @return pointer to command object, NULL if command not found
 */
struct cat_command const* cat_search_command_by_name(struct cat_object *self, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* CAT_H */
