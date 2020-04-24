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

#ifndef CAT_HOST_H
#define CAT_HOST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "cat_common.h"

/* enum type with main host at command manager fsm state */
typedef enum {
        CAT_HOST_STATE_ERROR = -1,
        CAT_HOST_STATE_IDLE,
} cat_host_state;

/* structure with host at command manager descriptor */
struct cat_host_descriptor {
        uint8_t *buf; /* pointer to working buffer (used to parse command argument) */
        size_t buf_size; /* working buffer length */
};

/* structure with main host at command manager object */
struct cat_host_object {
        struct cat_host_descriptor const *desc; /* pointer to at command parser descriptor */
        struct cat_io_interface const *io; /* pointer to at command parser io interface */
        struct cat_mutex_interface const *mutex; /* pointer to at command parser mutex interface */

        cat_host_state state; /* current fsm state */
};

/**
 * Function used to initialize host side at command manager.
 * Initialize starting values of object fields.
 * 
 * @param self pointer to host at command manager object to initialize
 * @param desc pointer to host at command manager descriptor
 * @param io pointer to host at command manager io low-level layer interface
 * @param mutex pointer to host at command manager mutex interface
 */
void cat_host_init(struct cat_host_object *self, const struct cat_host_descriptor *desc, const struct cat_io_interface *io, const struct cat_mutex_interface *mutex);

#ifdef __cplusplus
}
#endif

#endif /* CAT_HOST_H */
