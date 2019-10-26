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

#include "cat.h"

#include <string.h>
#include <assert.h>

static char to_upper(char ch)
{
        return (ch >= 'a' && ch <= 'z') ? ch - ('a' - 'A') : ch;
}

static void print_string(const struct cat_io_interface *iface, const char *str)
{
        assert(iface != NULL);
        assert(str != NULL);

        while (*str != 0)
                iface->write(*str++);
}

static void reset_state(struct cat_object *self)
{
        assert(self != NULL);

        self->state = CAT_STATE_PARSE_PREFIX;
        self->prefix_state = CAT_PREFIX_STATE_WAIT_A;
}

static void ack_error(struct cat_object *self)
{
        assert(self != NULL);

        print_string(self->iface, "\nERROR\n");
        reset_state(self);
}

static void ack_ok(struct cat_object *self)
{
        assert(self != NULL);

        print_string(self->iface, "\nOK\n");
        reset_state(self);
}

static int read_cmd_char(struct cat_object *self)
{
        assert(self != NULL);

        if (self->iface->read(&self->current_char) == 0)
                return 0;
        self->current_char = to_upper(self->current_char);
        return 1;
}

void cat_init(struct cat_object *self, const struct cat_descriptor *desc, const struct cat_io_interface *iface)
{
        size_t i;

        assert(self != NULL);
        assert(desc != NULL);
        assert(iface != NULL);

        assert(desc->cmd != NULL);
        assert(desc->cmd_num > 0);

        assert(desc->buf != NULL);
        assert(desc->buf_size * 4U >= desc->cmd_num);
        
        self->desc = desc;
        self->iface = iface;

        for (i = 0; i < self->desc->cmd_num; i++)
                assert(self->desc->cmd[i].name != NULL);
                
        reset_state(self);
}

static int error_state(struct cat_object *self)
{
        assert(self != NULL);

        if (read_cmd_char(self) == 0)
                return 0;

        switch (self->current_char) {
        case '\n':
                ack_error(self);
                break;
        default:
                break;
        }
        return 1;
}

static void prepare_parse_command(struct cat_object *self)
{
        assert(self != NULL);

        memset(self->desc->buf, 0x55, self->desc->buf_size);
        self->index = 0;
        self->cmd_current_length = 0;
        self->cmd_type = CAT_CMD_TYPE_EXECUTE;
}

static int parse_prefix(struct cat_object *self)
{
        assert(self != NULL);

        if (read_cmd_char(self) == 0)
                return 0;

        switch (self->prefix_state) {
        case CAT_PREFIX_STATE_WAIT_A:
                switch (self->current_char) {
                case 'A':
                        self->prefix_state = CAT_PREFIX_STATE_WAIT_T;
                        break;
                case '\n':
                case '\r':
                        break;
                default:
                        self->state = CAT_STATE_ERROR;
                        break;
                }
                break;
        case CAT_PREFIX_STATE_WAIT_T:
                switch (self->current_char) {
                case 'T':
                        prepare_parse_command(self);
                        self->state = CAT_STATE_PARSE_COMMAND_CHAR;
                        break;
                case '\n':
                        ack_error(self);
                        break;
                case '\r':
                        break;
                default:
                        self->state = CAT_STATE_ERROR;
                        break;
                }
                break;
        default:
                break;
        }
        return 1;
}

static void prepare_search_command(struct cat_object *self)
{
        assert(self != NULL);

        self->index = 0;        
        self->cmd_candidate_index = 0;
        self->cmd_found = false;
}

static int parse_command(struct cat_object *self)
{
        assert(self != NULL);

        if (read_cmd_char(self) == 0)
                return 0;

        switch (self->current_char) {
        case '\n':
                if (self->cmd_current_length != 0) {
                        prepare_search_command(self);
                        self->state = CAT_STATE_SEARCH_COMMAND;
                        break;
                }
                ack_ok(self);
                break;
        case '\r':
                break;
        case '?':
                if (self->cmd_current_length == 0) {
                        self->state = CAT_STATE_ERROR;
                        break;
                }
                self->cmd_type = CAT_CMD_TYPE_READ;
                self->state = CAT_STATE_WAIT_ACKNOWLEDGE;
                break;
        case '=':
                if (self->cmd_current_length == 0) {
                        self->state = CAT_STATE_ERROR;
                        break;
                }
                self->cmd_type = CAT_CMD_TYPE_WRITE;
                //TODO
                self->state = CAT_STATE_ERROR;
                break;
        default:
                if ((self->current_char >= 'A' && self->current_char <= 'Z') ||
                        (self->current_char >= '0' && self->current_char <= '9') ||
                        (self->current_char == '+')) {
                        self->cmd_current_length++;

                        self->state = CAT_STATE_UPDATE_COMMAND_STATE;
                        break;
                }
                self->state = CAT_STATE_ERROR;
                break;
        }
        return 1;
}

static uint8_t get_cmd_state(struct cat_object *self, size_t i)
{
        uint8_t s;

        s = self->desc->buf[i >> 2];
        s >>= (i % 4) << 1;
        s &= 0x03;

        return s;
}

static void set_cmd_state(struct cat_object *self, size_t i, uint8_t state)
{
        uint8_t s;
        size_t n;
        uint8_t k;

        n = i >> 2;
        k = ((i % 4) << 1);

        s = self->desc->buf[n];
        s &= ~(0x03 << k);
        s |= (state & 0x03) << k;
        self->desc->buf[n] = s;
}

static int update_command(struct cat_object *self)
{
        assert(self != NULL);

        struct cat_command const *cmd = &self->desc->cmd[self->index];
        size_t cmd_name_len;

        if (get_cmd_state(self, self->index) != CAT_CMD_STATE_NOT_MATCH) {
                cmd_name_len = strlen(cmd->name);

                if (self->cmd_current_length > cmd_name_len) {
                        set_cmd_state(self, self->index, CAT_CMD_STATE_NOT_MATCH);
                } else if (to_upper(cmd->name[self->cmd_current_length - 1]) != self->current_char) {
                        set_cmd_state(self, self->index, CAT_CMD_STATE_NOT_MATCH);
                } else if (self->cmd_current_length == cmd_name_len) {
                        set_cmd_state(self, self->index, CAT_CMD_STATE_FULL_MATCH);
                }
        }

        if (++self->index >= self->desc->cmd_num) {
                self->index = 0;
                self->state = CAT_STATE_PARSE_COMMAND_CHAR;
        }

        return 1;
}

static int wait_acknowledge(struct cat_object *self)
{
        assert(self != NULL);

        if (read_cmd_char(self) == 0)
                return 0;

        switch (self->current_char) {
        case '\n':
                prepare_search_command(self);
                self->state = CAT_STATE_SEARCH_COMMAND;
                break;
        case '\r':
                break;
        default:
                self->state = CAT_STATE_ERROR;
                break;
        }
        return 1;
}

static int search_command(struct cat_object *self)
{
        assert(self != NULL);

        uint8_t cmd_state = get_cmd_state(self, self->index);

        if (cmd_state != CAT_CMD_STATE_NOT_MATCH) {
                if (cmd_state == CAT_CMD_STATE_PARTIAL_MATCH) {
                        if (self->cmd_found != false) {
                                self->state = CAT_STATE_COMMAND_NOT_FOUND;
                                return 1;
                        }
                        self->cmd_candidate_index = self->index;
                        self->cmd_found = true;
                } else if (cmd_state == CAT_CMD_STATE_FULL_MATCH) {
                        self->cmd_candidate_index = self->index;
                        self->state = CAT_STATE_COMMAND_FOUND;
                        return 1;
                }
        }

        if (++self->index >= self->desc->cmd_num) {
                if (self->cmd_found == false) {
                        self->state = CAT_STATE_COMMAND_NOT_FOUND;
                } else {
                        self->state = CAT_STATE_COMMAND_FOUND;
                }
        }

        return 1;
}

static int command_found(struct cat_object *self)
{
        size_t size;

        assert(self != NULL);

        struct cat_command const *cmd = &self->desc->cmd[self->cmd_candidate_index];

        switch (self->cmd_type) {
        case CAT_CMD_TYPE_EXECUTE:
                if (cmd->run == NULL) {
                        ack_error(self);
                        break;
                }

                if (cmd->run(cmd->name) != 0) {
                        ack_error(self);
                        break;    
                }

                ack_ok(self);
                break;
        case CAT_CMD_TYPE_READ:
                if (cmd->read == NULL) {
                        ack_error(self);
                        break;
                }
                if (cmd->read(cmd->name, self->desc->buf, &size, self->desc->buf_size) != 0) {
                        ack_error(self);
                        break;
                }
                print_string(self->iface, "\n");
                print_string(self->iface, cmd->name);
                print_string(self->iface, "=");

                self->buf_current_index = 0;
                while (size-- > 0)
                        self->iface->write(self->desc->buf[self->buf_current_index++]);

                print_string(self->iface, "\n");

                ack_ok(self);
                break;
        case CAT_CMD_TYPE_WRITE:
                ack_error(self);
                break;
        
        }

        return 1;       
}

static int command_not_found(struct cat_object *self)
{
        assert(self != NULL);

        ack_error(self);
        return 1;
}

int cat_service(struct cat_object *self)
{
        assert(self != NULL);

        switch (self->state) {
        case CAT_STATE_ERROR:
                return error_state(self);
        case CAT_STATE_PARSE_PREFIX:
                return parse_prefix(self);
        case CAT_STATE_PARSE_COMMAND_CHAR:
                return parse_command(self);
        case CAT_STATE_UPDATE_COMMAND_STATE:
                return update_command(self);
        case CAT_STATE_WAIT_ACKNOWLEDGE:
                return wait_acknowledge(self);
        case CAT_STATE_SEARCH_COMMAND:
                return search_command(self);
        case CAT_STATE_COMMAND_FOUND:
                return command_found(self);
        case CAT_STATE_COMMAND_NOT_FOUND:
                return command_not_found(self);
        default:
                break;
        }

        return 1;
}
