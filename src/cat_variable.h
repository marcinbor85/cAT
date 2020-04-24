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

#ifndef CAT_VARIABLE_H
#define CAT_VARIABLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "cat_common.h"

/* only forward declarations (looks for definition below) */
struct cat_variable;

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

        cat_var_write_handler write; /* write variable handler */
        cat_var_read_handler read; /* read variable handler */
};

#ifdef __cplusplus
}
#endif

#endif /* CAT_VARIABLE_H */
