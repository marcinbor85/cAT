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

#define CAT_CMD_STATE_NOT_MATCH          (0)
#define CAT_CMD_STATE_PARTIAL_MATCH      (1U)
#define CAT_CMD_STATE_FULL_MATCH         (2U)

static char to_upper(char ch)
{
        return (ch >= 'a' && ch <= 'z') ? ch - ('a' - 'A') : ch;
}

static void print_string(const struct cat_io_interface *iface, const char *str)
{
        assert(iface != NULL);
        assert(str != NULL);

        while (*str != 0) {
                while (iface->write(*str) != 1) {};
                str++;
        }
}

static void print_binary(const struct cat_io_interface *iface, const uint8_t *data, size_t size)
{
        size_t i;

        assert(iface != NULL);
        assert(data != NULL);

        i = 0;
        while (size-- > 0) {
                while (iface->write(data[i]) != 1) {};
                i++;
        }
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
        
        if (self->state != CAT_STATE_PARSE_COMMAND_ARGS)
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
        assert(desc->buf_size > 0);

        assert(desc->state_buf != NULL);
        assert(desc->state_buf_size * 4U >= desc->cmd_num);
        
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

        memset(
                self->desc->state_buf,
                (CAT_CMD_STATE_PARTIAL_MATCH << 0) |
                (CAT_CMD_STATE_PARTIAL_MATCH << 2) |
                (CAT_CMD_STATE_PARTIAL_MATCH << 4) |
                (CAT_CMD_STATE_PARTIAL_MATCH << 6),
                self->desc->state_buf_size
        );

        self->index = 0;
        self->length = 0;
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
        self->cmd = NULL;
}

static int is_valid_cmd_name_char(const char ch)
{
        return (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || (ch == '+') || (ch == '#') || (ch == '$') || (ch == '@');
}

static int is_valid_dec_char(const char ch)
{
        return (ch >= '0' && ch <= '9');
}

static int is_valid_hex_char(const char ch)
{
        return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F');
}

static int parse_command(struct cat_object *self)
{
        assert(self != NULL);

        if (read_cmd_char(self) == 0)
                return 0;

        switch (self->current_char) {
        case '\n':
                if (self->length != 0) {
                        prepare_search_command(self);
                        self->state = CAT_STATE_SEARCH_COMMAND;
                        break;
                }
                ack_ok(self);
                break;
        case '\r':
                break;
        case '?':
                if (self->length == 0) {
                        self->state = CAT_STATE_ERROR;
                        break;
                }
                self->cmd_type = CAT_CMD_TYPE_READ;
                self->state = CAT_STATE_WAIT_ACKNOWLEDGE;
                break;
        case '=':
                if (self->length == 0) {
                        self->state = CAT_STATE_ERROR;
                        break;
                }
                self->cmd_type = CAT_CMD_TYPE_WRITE;
                prepare_search_command(self);
                self->state = CAT_STATE_SEARCH_COMMAND;
                break;
        default:
                if (is_valid_cmd_name_char(self->current_char) != 0) {
                        self->length++;
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

        s = self->desc->state_buf[i >> 2];
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

        s = self->desc->state_buf[n];
        s &= ~(0x03 << k);
        s |= (state & 0x03) << k;
        self->desc->state_buf[n] = s;
}

static int update_command(struct cat_object *self)
{
        assert(self != NULL);

        struct cat_command const *cmd = &self->desc->cmd[self->index];
        size_t cmd_name_len;

        if (get_cmd_state(self, self->index) != CAT_CMD_STATE_NOT_MATCH) {
                cmd_name_len = strlen(cmd->name);

                if (self->length > cmd_name_len) {
                        set_cmd_state(self, self->index, CAT_CMD_STATE_NOT_MATCH);
                } else if (to_upper(cmd->name[self->length - 1]) != self->current_char) {
                        set_cmd_state(self, self->index, CAT_CMD_STATE_NOT_MATCH);
                } else if (self->length == cmd_name_len) {
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
                        if (self->cmd != NULL) {
                                self->state = (self->current_char == '\n') ? CAT_STATE_COMMAND_NOT_FOUND : CAT_STATE_ERROR;
                                return 1;
                        }
                        self->cmd = &self->desc->cmd[self->index];
                } else if (cmd_state == CAT_CMD_STATE_FULL_MATCH) {
                        self->cmd = &self->desc->cmd[self->index];
                        self->state = CAT_STATE_COMMAND_FOUND;
                        return 1;
                }
        }

        if (++self->index >= self->desc->cmd_num) {
                if (self->cmd == NULL) {
                        self->state = (self->current_char == '\n') ? CAT_STATE_COMMAND_NOT_FOUND : CAT_STATE_ERROR;
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

        switch (self->cmd_type) {
        case CAT_CMD_TYPE_EXECUTE:
                if (self->cmd->run == NULL) {
                        ack_error(self);
                        break;
                }

                if (self->cmd->run(self->cmd) != 0) {
                        ack_error(self);
                        break;    
                }

                ack_ok(self);
                break;
        case CAT_CMD_TYPE_READ:
                if (self->cmd->read == NULL) {
                        ack_error(self);
                        break;
                }
                if (self->cmd->read(self->cmd, self->desc->buf, &size, self->desc->buf_size) != 0) {
                        ack_error(self);
                        break;
                }
                print_string(self->iface, "\n");
                print_string(self->iface, self->cmd->name);
                print_string(self->iface, "=");
                print_binary(self->iface, self->desc->buf, size);
                print_string(self->iface, "\n");

                ack_ok(self);
                break;
        case CAT_CMD_TYPE_WRITE:
                self->length = 0;
                self->desc->buf[0] = 0;
                self->state = CAT_STATE_PARSE_COMMAND_ARGS;
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

static int parse_int_decimal(struct cat_object *self, int64_t *ret)
{
        assert(self != NULL);
        assert(ret != NULL);

        char ch;
        int64_t val = 0;
        int64_t sign = 0;
        int ok = 0;

        while (1) {
                ch = self->desc->buf[self->position++];

                if ((ok != 0) && ((ch == 0) || (ch == ','))) {
                        val *= sign;
                        *ret = val;
                        return (ch == ',') ? 1 : 0;
                }

                if (sign == 0) {
                        if (ch == '-') {
                                sign = -1;
                        } else if (ch == '+') {
                                sign = 1;
                        } else if (is_valid_dec_char(ch) != 0) {
                                sign = 1;
                                val = ch - '0';
                                ok = 1;
                        } else {
                                return -1;
                        }
                } else {
                        if (is_valid_dec_char(ch) != 0) {
                                ok = 1;
                                val *= 10;
                                val += ch - '0';
                        } else {
                                return -1;
                        }
                }
        }

        return -1;
}

static int parse_uint_decimal(struct cat_object *self, uint64_t *ret)
{
        assert(self != NULL);
        assert(ret != NULL);

        char ch;
        uint64_t val = 0;
        int ok = 0;

        while (1) {
                ch = self->desc->buf[self->position++];

                if ((ok != 0) && ((ch == 0) || (ch == ','))) {
                        *ret = val;
                        return (ch == ',') ? 1 : 0;
                }

                if (is_valid_dec_char(ch) != 0) {
                        ok = 1;
                        val *= 10;
                        val += ch - '0';
                } else {
                        return -1;
                }
        }

        return -1;
}

static int parse_num_hexadecimal(struct cat_object *self, uint64_t *ret)
{
        assert(self != NULL);
        assert(ret != NULL);

        char ch;
        uint64_t val = 0;
        int state = 0;

        while (1) {
                ch = self->desc->buf[self->position++];
                ch = to_upper(ch);

                if ((state >= 3) && ((ch == 0) || (ch == ','))) {
                        *ret = val;
                        return (ch == ',') ? 1 : 0;
                }

                if (state == 0) {
                        if (ch != '0')
                                return -1;
                        state = 1;
                } else if (state == 1) {
                        if (ch != 'X')
                                return -1;
                        state = 2;
                } else if (state >= 2) {
                        if (is_valid_hex_char(ch) != 0) {
                                state = 3;
                                val <<= 4;
                                val += ((ch >= '0') && (ch <= '9')) ? (uint8_t)(ch - '0') : (uint8_t)(ch - 'A' + 10U);
                        } else {
                                return -1;
                        }
                }
        }

        return -1;
}

static int parse_buffer_hexadecimal(struct cat_object *self)
{
        assert(self != NULL);

        char ch;
        uint8_t byte = 0;
        int state = 0;
        size_t size = 0;

        while (1) {
                ch = self->desc->buf[self->position++];
                ch = to_upper(ch);

                if ((size > 0) && (state == 0) && ((ch == 0) || (ch == ','))) {
                        self->write_size = size;
                        return (ch == ',') ? 1 : 0;
                }

                if (is_valid_hex_char(ch) == 0)
                        return -1;
                
                byte <<= 4;
                byte += ((ch >= '0') && (ch <= '9')) ? (uint8_t)(ch - '0') : (uint8_t)(ch - 'A' + 10U);

                if (state != 0) {
                        ((uint8_t*)(self->var->data))[size++] = byte;
                        byte = 0;

                        if (size > self->var->data_size)
                                return -1;
                }
                
                state = !state;
        }

        return -1;
}

static int parse_buffer_string(struct cat_object *self)
{
        assert(self != NULL);

        char ch;
        int state = 0;
        size_t size = 0;

        while (1) {
                ch = self->desc->buf[self->position++];
                
                switch (state) {
                case 0:
                        if (ch != '"')
                                return -1;
                        state = 1;              
                        break;
                case 1:
                        if (ch == 0)
                                return -1;
                        if (ch == '\\') {
                                state = 2;
                                break;
                        }
                        if (ch == '"') {
                                state = 3;
                                break;
                        }
                        if (size >= self->var->data_size)
                                return -1;                                
                        ((uint8_t*)(self->var->data))[size++] = ch;
                        break;
                case 2:
                        if ((ch == '\\') || (ch == '"')) {
                                if (size >= self->var->data_size)
                                        return -1;                                
                                ((uint8_t*)(self->var->data))[size++] = ch;
                                state = 1;
                        } else {
                                return -1;
                        }
                        break;
                case 3:
                        if ((ch == 0) || (ch == ',')) {
                                if (size >= self->var->data_size)
                                        return -1;
                                ((uint8_t*)(self->var->data))[size] = 0;
                                self->write_size = size;
                                return (ch == ',') ? 1 : 0;
                        } else {
                                return -1;
                        }
                        break;
                }
        }

        return -1;
}

static int validate_int_range(struct cat_object *self, int64_t val)
{
        switch (self->var->data_size) {
        case 1:
                if ((val < INT8_MIN) || (val > INT8_MAX))
                        return -1;
                *(int8_t*)(self->var->data) = val;
                break;
        case 2:
                if ((val < INT16_MIN) || (val > INT16_MAX))
                        return -1;
                *(int16_t*)(self->var->data) = val;
                break;
        case 4:
                if ((val < INT32_MIN) || (val > INT32_MAX))
                        return -1;
                *(int32_t*)(self->var->data) = val;
                break;
        default:
                return -1;
        }
        self->write_size = self->var->data_size;
        return 0;
}

static int validate_uint_range(struct cat_object *self, uint64_t val)
{
        switch (self->var->data_size) {
        case 1:
                if (val > UINT8_MAX)
                        return -1;
                *(uint8_t*)(self->var->data) = val;
                break;
        case 2:
                if (val > UINT16_MAX)
                        return -1;
                *(uint16_t*)(self->var->data) = val;
                break;
        case 4:
                if (val > UINT32_MAX)
                        return -1;
                *(uint32_t*)(self->var->data) = val;
                break;
        default:
                return -1;
        }
        self->write_size = self->var->data_size;
        return 0;
}

static int parse_write_args(struct cat_object *self)
{
        int64_t val;
        int stat;

        assert(self != NULL);

        switch (self->var->type) {
        case CAT_VAR_INT_DEC:
                stat = parse_int_decimal(self, &val);
                if (stat < 0) {
                        ack_error(self);
                        return -1;
                }
                if (validate_int_range(self, val) != 0) {
                        ack_error(self);
                        return -1;
                }
                break;
        case CAT_VAR_UINT_DEC:
                stat = parse_uint_decimal(self, (uint64_t*)&val);
                if (stat < 0) {
                        ack_error(self);
                        return -1;
                }
                if (validate_uint_range(self, val) != 0) {
                        ack_error(self);
                        return -1;
                }
                break;
        case CAT_VAR_NUM_HEX:
                stat = parse_num_hexadecimal(self, (uint64_t*)&val);
                if (stat < 0) {
                        ack_error(self);
                        return -1;
                }
                if (validate_uint_range(self, val) != 0) {
                        ack_error(self);
                        return -1;
                }
                break;
        case CAT_VAR_BUF_HEX:
                stat = parse_buffer_hexadecimal(self);
                if (stat < 0) {
                        ack_error(self);
                        return -1;
                }
                break;
        case CAT_VAR_BUF_STRING:
                stat = parse_buffer_string(self);
                if (stat < 0) {
                        ack_error(self);
                        return -1;
                }
                break;
        }

        if ((self->var->write != NULL) && (self->var->write(self->var, self->write_size) != 0)) {
                ack_error(self);
                return -1;
        }

        if ((++self->index < self->cmd->var_num) && (stat > 0)) {
                self->var = &self->cmd->var[self->index];
                return 1;
        }

        if (stat > 0) {
                ack_error(self);
                return -1;
        }

        if ((self->cmd->need_all_vars != false) && (self->index != self->cmd->var_num)) {
                ack_error(self);
                return -1;
        }

        if (self->cmd->write == NULL) {
                ack_ok(self);
                return 1;
        }

        if (self->cmd->write(self->cmd, self->desc->buf, self->length, self->index) != 0) {
                ack_error(self);
                return 1;
        }

        ack_ok(self);
        return 1;
}

static int parse_command_args(struct cat_object *self)
{
        assert(self != NULL);

        if (read_cmd_char(self) == 0)
                return 0;

        switch (self->current_char) {
        case '\n':
                if ((self->cmd->var != NULL) && (self->cmd->var_num > 0)) {
                        self->state = CAT_STATE_PARSE_WRITE_ARGS;
                        self->position = 0;
                        self->index = 0;
                        self->var = &self->cmd->var[self->index];
                        break;
                }
                if (self->cmd->write == NULL) {
                        ack_error(self);
                        break;
                }
                if (self->cmd->write(self->cmd, self->desc->buf, self->length, 0) != 0) {
                        ack_error(self);
                        break;
                }
                ack_ok(self);
                break;
        case '\r':
                break;
        default:
                if (self->length >= self->desc->buf_size) {
                        self->state = CAT_STATE_ERROR;
                        break;
                }
                self->desc->buf[self->length++] = self->current_char;
                if (self->length < self->desc->buf_size) {
                        self->desc->buf[self->length] = 0;
                } else {
                        self->state = CAT_STATE_ERROR;
                }
                break;
        }
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
        case CAT_STATE_PARSE_COMMAND_ARGS:
                return parse_command_args(self);
        case CAT_STATE_PARSE_WRITE_ARGS:
                return parse_write_args(self);
        default:
                break;
        }

        return 1;
}
