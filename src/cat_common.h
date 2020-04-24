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

#ifndef CAT_COMMON_H
#define CAT_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

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
} cat_return_state;

/* enum type with variable type definitions */
typedef enum {
        CAT_VAR_INT_DEC = 0, /* decimal encoded signed integer variable */
        CAT_VAR_UINT_DEC, /* decimal encoded unsigned integer variable */
        CAT_VAR_NUM_HEX, /* hexadecimal encoded unsigned integer variable */
        CAT_VAR_BUF_HEX, /* asciihex encoded bytes array */
        CAT_VAR_BUF_STRING /* string variable */
} cat_var_type;

/* enum type with function status */
typedef enum {
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

/* enum type with type of command request */
typedef enum {
        CAT_CMD_TYPE_RUN = 0,
        CAT_CMD_TYPE_READ,
        CAT_CMD_TYPE_WRITE,
        CAT_CMD_TYPE_TEST
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

#ifdef __cplusplus
}
#endif

#endif /* CAT_COMMON_H */
