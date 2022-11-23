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
#include <stddef.h>
#include <stdbool.h>

/* only forward declarations (looks for definition below) */
struct cat_command;
struct cat_variable;

#ifndef CAT_UNSOLICITED_CMD_BUFFER_SIZE
/* unsolicited command buffer default size (can by override externally during compilation) */
#define CAT_UNSOLICITED_CMD_BUFFER_SIZE     ((size_t)(1))
#endif

/* enum type with variable type definitions */
typedef enum {
        CAT_VAR_INT_DEC = 0, /* decimal encoded signed integer variable */
        CAT_VAR_UINT_DEC, /* decimal encoded unsigned integer variable */
        CAT_VAR_NUM_HEX, /* hexadecimal encoded unsigned integer variable */
        CAT_VAR_BUF_HEX, /* asciihex encoded bytes array */
        CAT_VAR_BUF_STRING /* string variable */
} cat_var_type;

/* enum type with variable accessors definitions */
typedef enum {
        CAT_VAR_ACCESS_READ_WRITE = 0, /* there will be possible to read and write variable */
        CAT_VAR_ACCESS_READ_ONLY, /* there will be possible to read only variable */
        CAT_VAR_ACCESS_WRITE_ONLY, /* there will be possible to write only variable */
} cat_var_access;

/* enum type with function status */
typedef enum {
        CAT_STATUS_ERROR_BUFFER_EMPTY = -7,
        CAT_STATUS_ERROR_NOT_HOLD = -6,
        CAT_STATUS_ERROR_BUFFER_FULL = -5,
        CAT_STATUS_ERROR_UNKNOWN_STATE = -4,
        CAT_STATUS_ERROR_MUTEX_LOCK = -3,
        CAT_STATUS_ERROR_MUTEX_UNLOCK = -2,
        CAT_STATUS_ERROR = -1,
        CAT_STATUS_OK = 0,
        CAT_STATUS_BUSY = 1,
        CAT_STATUS_HOLD = 2
} cat_status;

/**
 * Write variable function handler
 * 
 * This callback function is called after variable update immediatly.
 * User application can validate writed value and check for amount of data size was written.
 * This handler is optional, so when is not defined, operations will be fully automatically.
 * 
 * @param var - pointer to struct descriptor of parsed variable
 * @param write_size - size of data was written (useful especially with hexbuf var type)
 * @return 0 - ok, else error and stop parsing
 * */
typedef int (*cat_var_write_handler)(const struct cat_variable *var, const size_t write_size);

/**
 * Read variable function handler
 * 
 * This callback function is called just before variable value read.
 * User application can copy data from private fields to variable connected with parsed command.
 * This handler is optional, so when is not defined, operations will be fully automatically.
 * 
 * @param var - pointer to struct descriptor of parsed variable
 * @return 0 - ok, else error and stop parsing
 * */
typedef int (*cat_var_read_handler)(const struct cat_variable *var);

struct cat_variable {
        const char *name; /* variable name (optional - using only for auto format test command response) */
        cat_var_type type; /* variable type (needed for parsing and validating) */
        void *data; /* generic pointer to statically allocated memory for variable data read/write/validate operations */
        size_t data_size; /* variable data size, pointed by data pointer */
        cat_var_access access; /* variable accessor */

        cat_var_write_handler write; /* write variable handler */
        cat_var_read_handler read; /* read variable handler */
};

/* enum type with command callbacks return values meaning */
typedef enum {
        CAT_RETURN_STATE_ERROR = -1, /* immediatly error acknowledge */
        CAT_RETURN_STATE_DATA_OK, /* send current data buffer followed by ok acknowledge */
        CAT_RETURN_STATE_DATA_NEXT, /* send current data buffer and go to next callback iteration */
        CAT_RETURN_STATE_NEXT, /* go to next callback iteration without sending anything */
        CAT_RETURN_STATE_OK, /* immediatly ok acknowledge */
        CAT_RETURN_STATE_HOLD, /* enable hold parser state */
        CAT_RETURN_STATE_HOLD_EXIT_OK, /* exit from hold state with OK response */
        CAT_RETURN_STATE_HOLD_EXIT_ERROR, /* exit from hold state with ERROR response */
        CAT_RETURN_STATE_PRINT_CMD_LIST_OK, /* print commands list followed by ok acknowledge (only in TEST and RUN) */
} cat_return_state;

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
        CAT_STATE_FLUSH_IO_WRITE_WAIT,
        CAT_STATE_FLUSH_IO_WRITE,
        CAT_STATE_AFTER_FLUSH_RESET,
        CAT_STATE_AFTER_FLUSH_OK,
        CAT_STATE_AFTER_FLUSH_FORMAT_READ_ARGS,
        CAT_STATE_AFTER_FLUSH_FORMAT_TEST_ARGS,
        CAT_STATE_PRINT_CMD,
} cat_state;

/* enum type with type of command request */
typedef enum {
        CAT_CMD_TYPE_NONE = -1,
        CAT_CMD_TYPE_RUN,
        CAT_CMD_TYPE_READ,
        CAT_CMD_TYPE_WRITE,
        CAT_CMD_TYPE_TEST,
        CAT_CMD_TYPE__TOTAL_NUM
} cat_cmd_type;

/* structure with io interface functions */
struct cat_io_interface {
        int (*write)(char ch); /* write char to output stream. return 1 if byte wrote successfully. */
        int (*read)(char *ch); /* read char from input stream. return 1 if byte read successfully. */
};

/* structure with mutex interface functions */
struct cat_mutex_interface {
        int (*lock)(void); /* lock mutex handler. return 0 if successfully locked, otherwise - cannot lock */
        int (*unlock)(void); /* unlock mutex handler. return 0 if successfully unlocked, otherwise - cannot unlock */
};

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
        bool disable; /* flag to completely disable command */
        bool implicit_write; /* flag to mark command as implicit write */
};

struct cat_command_group {
        const char *name; /* command group name (optional, for identification purpose) */

        struct cat_command const *cmd; /* pointer to array of commands descriptor */
        size_t cmd_num; /* number of commands in array */

        bool disable; /* flag to completely disable all commands in group */
};

/* structure with at command parser descriptor */
struct cat_descriptor {
        struct cat_command_group* const *cmd_group; /* pointer to array of commands group descriptor */
        size_t cmd_group_num; /* number of commands group in array */

        uint8_t *buf; /* pointer to working buffer (used to parse command argument) */
        size_t buf_size; /* working buffer length */

        /* optional unsolicited buffer, if not configured (NULL) */
        /* then the buf will be divided into two smaller buffers */
        uint8_t *unsolicited_buf; /* pointer to unsolicited working buffer (used to parse command argument) */
        size_t unsolicited_buf_size; /* unsolicited working buffer length */
};

/* strcuture with unsolicited command buffered infos */
struct cat_unsolicited_cmd {
        struct cat_command const *cmd; /* pointer to commands used to unsolicited event */
        cat_cmd_type type; /* type of unsolicited event */
};

/* enum type with unsolicited events fsm state */
typedef enum {
        CAT_UNSOLICITED_STATE_IDLE,
        CAT_UNSOLICITED_STATE_FORMAT_READ_ARGS,
        CAT_UNSOLICITED_STATE_FORMAT_TEST_ARGS,
        CAT_UNSOLICITED_STATE_READ_LOOP,
        CAT_UNSOLICITED_STATE_TEST_LOOP,
        CAT_UNSOLICITED_STATE_FLUSH_IO_WRITE_WAIT,
        CAT_UNSOLICITED_STATE_FLUSH_IO_WRITE,
        CAT_UNSOLICITED_STATE_AFTER_FLUSH_RESET,
        CAT_UNSOLICITED_STATE_AFTER_FLUSH_OK,
        CAT_UNSOLICITED_STATE_AFTER_FLUSH_FORMAT_READ_ARGS,
        CAT_UNSOLICITED_STATE_AFTER_FLUSH_FORMAT_TEST_ARGS,
} cat_unsolicited_state;

/* enum type with fsm type */
typedef enum {
        CAT_FSM_TYPE_ATCMD,
        CAT_FSM_TYPE_UNSOLICITED,
        CAT_FSM_TYPE__TOTAL_NUM,
} cat_fsm_type;

struct cat_unsolicited_fsm {
        cat_unsolicited_state state; /* current unsolicited fsm state */

        size_t index; /* index used to iterate over commands and variables */
        size_t position; /* position of actually parsed char in arguments string */

        struct cat_command const *cmd; /* pointer to current command descriptor */
        struct cat_variable const *var; /* pointer to current variable descriptor */
        cat_cmd_type cmd_type; /* type of command request */

        char const *write_buf; /* working buffer pointer used for asynch writing to io */
        int write_state; /* before, data, after flush io write state */
        cat_unsolicited_state write_state_after; /* parser state to set after flush io write */

        struct cat_unsolicited_cmd unsolicited_cmd_buffer[CAT_UNSOLICITED_CMD_BUFFER_SIZE]; /* buffer with unsolicited commands used to unsolicited event */
        size_t unsolicited_cmd_buffer_tail; /* tail index of unsolicited cmd buffer */
        size_t unsolicited_cmd_buffer_head; /* head index of unsolicited cmd buffer */
        size_t unsolicited_cmd_buffer_items_count; /* number of unsolicited cmd in buffer */
};

/* structure with main at command parser object */
struct cat_object {
        struct cat_descriptor const *desc; /* pointer to at command parser descriptor */
        struct cat_io_interface const *io; /* pointer to at command parser io interface */
        struct cat_mutex_interface const *mutex; /* pointer to at command parser mutex interface */

        size_t index; /* index used to iterate over commands and variables */
        size_t partial_cntr; /* partial match commands counter */
        size_t length; /* length of input command name and command arguments */
        size_t position; /* position of actually parsed char in arguments string */
        size_t write_size; /* size of parsed buffer hex or buffer string */
        size_t commands_num; /* computed total number of registered commands */

        struct cat_command const *cmd; /* pointer to current command descriptor */
        struct cat_variable const *var; /* pointer to current variable descriptor */
        cat_cmd_type cmd_type; /* type of command request */

        char current_char; /* current received char from input stream */
        cat_state state; /* current fsm state */
        bool cr_flag; /* flag for detect <cr> char in input string */
        bool hold_state_flag; /* status of hold state (independent from fsm states) */
        int hold_exit_status; /* hold exit parameter with status */
        char const *write_buf; /* working buffer pointer used for asynch writing to io */
        int write_state; /* before, data, after flush io write state */
        cat_state write_state_after; /* parser state to set after flush io write */
        bool implicit_write_flag; /* flag that implicit write was detected */

        struct cat_unsolicited_fsm unsolicited_fsm;
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
 * Function return flag which indicating state of internal buffer of unsolicited events.
 * 
 * @param self pointer to at command parser object
 * @return CAT_STATUS_OK - buffer is not full, unsolicited event can be buffered
 *         CAT_STATUS_ERROR_BUFFER_FULL - buffer is full, unsolicited event cannot be buffered
 *         CAT_STATUS_ERROR_MUTEX_LOCK - cannot lock mutex error
 *         CAT_STATUS_ERROR_MUTEX_UNLOCK - cannot unlock mutex error
 */
cat_status cat_is_unsolicited_buffer_full(struct cat_object *self);

/**
 * Function sends unsolicited event message.
 * Command message is buffered inside parser in 1-level deep buffer and processed in cat_service context.
 * Only command pointer is buffered, so command struct should be static or global until be fully processed.
 * 
 * @param self pointer to at command parser object
 * @param cmd pointer to command structure regarding which unsolicited event applies to
 * @param type type of operation (only CAT_CMD_TYPE_READ and CAT_CMD_TYPE_TEST are allowed)
 * @return according to cat_return_state enum definitions
 */
cat_status cat_trigger_unsolicited_event(struct cat_object *self, struct cat_command const *cmd, cat_cmd_type type);

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

/**
 * Function used to searching registered command group by its name.
 * 
 * @param self pointer to at command parser object
 * @param name command group name to search
 * @return pointer to command group object, NULL if command group not found
 */
struct cat_command_group const* cat_search_command_group_by_name(struct cat_object *self, const char *name);

/**
 * Function used to searching attached variable to command its name.
 * 
 * @param self pointer to at command parser object
 * @param cmd pointer to command in which variable will be searched
 * @param name variable name to search
 * @return pointer to command group object, NULL if command group not found
 */
struct cat_variable const* cat_search_variable_by_name(struct cat_object *self, struct cat_command const *cmd, const char *name);

/**
 * Function used to check what command is currently processed.
 * Function is not protected by mutex mechanism, due to processed cmd may change after function return.
 * This only matters in multithreaded environments, it does not matter for one thread.
 * 
 * @param self pointer to at command parser object
 * @param fsm type of internal state machine to check current command
 * @return pointer to command which is currently processed, NULL if no command is processed
 */
struct cat_command const* cat_get_processed_command(struct cat_object *self, cat_fsm_type fsm);

/**
 * Function return unsolicited event command status.
 * Function is not protected by mutex mechanism, due to processed cmd may change after function return.
 * This only matters in multithreaded environments, it does not matter for one thread.
 * 
 * @param self pointer to at command parser object
 * @param cmd pointer to command in which variable will be searched
 * @param type type of unsolicited event
 * @return CAT_STATUS_OK - command is not buffered nor processed
 *         CAT_STATUS_BUSY - command is waiting in buffer or is processed
 */
cat_status cat_is_unsolicited_event_buffered(struct cat_object *self, struct cat_command const *cmd, cat_cmd_type type);

#ifdef __cplusplus
}
#endif

#endif /* CAT_H */
