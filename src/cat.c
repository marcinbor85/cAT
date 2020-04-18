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

#define CAT_CMD_STATE_NOT_MATCH (0)
#define CAT_CMD_STATE_PARTIAL_MATCH (1U)
#define CAT_CMD_STATE_FULL_MATCH (2U)

static char to_upper(char ch)
{
        return (ch >= 'a' && ch <= 'z') ? ch - ('a' - 'A') : ch;
}

static void print_string(const struct cat_io_interface *io, const char *str)
{
        assert(io != NULL);
        assert(str != NULL);

        while (*str != 0) {
                while (io->write(*str) != 1) {
                };
                str++;
        }
}

static void print_binary(const struct cat_io_interface *io, const uint8_t *data, size_t size)
{
        size_t i;

        assert(io != NULL);
        assert(data != NULL);

        i = 0;
        while (size-- > 0) {
                while (io->write(data[i]) != 1) {
                };
                i++;
        }
}

static void reset_state(struct cat_object *self)
{
        assert(self != NULL);

        if (self->hold_state_flag == false) {
                self->state = CAT_STATE_IDLE;
                self->cr_flag = false;
        } else {
                self->state = CAT_STATE_HOLD;
        }
        self->disable_ack = false;
}

static cat_status is_busy(struct cat_object *self)
{
        return (self->state != CAT_STATE_IDLE) ? CAT_STATUS_BUSY : CAT_STATUS_OK;
}

cat_status cat_is_busy(struct cat_object *self)
{
        cat_status s;

        assert(self != NULL);

        if ((self->mutex != NULL) && (self->mutex->lock() != 0))
                return CAT_STATUS_ERROR_MUTEX_LOCK;

        s = is_busy(self);

        if ((self->mutex != NULL) && (self->mutex->unlock() != 0))
                return CAT_STATUS_ERROR_MUTEX_UNLOCK;

        return s;
}

static const char *get_new_line_chars(struct cat_object *self)
{
        static const char *crlf = "\r\n";
        return &crlf[(self->cr_flag != false) ? 0 : 1];
}

static void print_new_line(struct cat_object *self)
{
        assert(self != NULL);

        print_string(self->io, get_new_line_chars(self));
}

static void print_line(struct cat_object *self, const char *buf)
{
        assert(self != NULL);

        print_new_line(self);
        print_string(self->io, buf);
        print_new_line(self);
}

static void ack_error(struct cat_object *self)
{
        assert(self != NULL);

        if (self->disable_ack == false)
                print_line(self, "ERROR");

        reset_state(self);
}

static void ack_ok(struct cat_object *self)
{
        assert(self != NULL);

        if (self->disable_ack == false)
                print_line(self, "OK");

        reset_state(self);
}

static void print_buffer(struct cat_object *self)
{
        assert(self != NULL);

        print_new_line(self);
        print_string(self->io, self->cmd->name);
        print_string(self->io, "=");
        print_binary(self->io, self->desc->buf, self->position);
        print_new_line(self);
}

static int print_string_to_buf(struct cat_object *self, const char *str)
{
        int written;
        size_t len;

        len = self->desc->buf_size - self->position;
        written = snprintf((char *)&self->desc->buf[self->position], len, "%s", str);

        if ((written < 0) || ((size_t)written >= len))
                return -1;

        self->position += written;
        return 0;
}

static int read_cmd_char(struct cat_object *self)
{
        assert(self != NULL);

        if (self->io->read(&self->current_char) == 0)
                return 0;

        if (self->state != CAT_STATE_PARSE_COMMAND_ARGS)
                self->current_char = to_upper(self->current_char);

        return 1;
}

void cat_init(struct cat_object *self, const struct cat_descriptor *desc, const struct cat_io_interface *io, const struct cat_mutex_interface *mutex)
{
        size_t i;

        assert(self != NULL);
        assert(desc != NULL);
        assert(io != NULL);

        assert(desc->cmd != NULL);
        assert(desc->cmd_num > 0);

        assert(desc->buf != NULL);
        assert(desc->buf_size * 4U >= desc->cmd_num);

        self->desc = desc;
        self->io = io;
        self->mutex = mutex;
        self->unsolicited_read_cmd = NULL;
        self->hold_state_flag = false;
        self->hold_exit_status = 0;

        for (i = 0; i < self->desc->cmd_num; i++)
                assert(self->desc->cmd[i].name != NULL);

        reset_state(self);
}

static cat_status error_state(struct cat_object *self)
{
        assert(self != NULL);

        if (read_cmd_char(self) == 0)
                return CAT_STATUS_OK;

        switch (self->current_char) {
        case '\n':
                ack_error(self);
                break;
        case '\r':
                self->cr_flag = true;
                break;
        default:
                break;
        }
        return CAT_STATUS_BUSY;
}

static void prepare_parse_command(struct cat_object *self)
{
        uint8_t val = (CAT_CMD_STATE_PARTIAL_MATCH << 0) | (CAT_CMD_STATE_PARTIAL_MATCH << 2) | (CAT_CMD_STATE_PARTIAL_MATCH << 4) |
                      (CAT_CMD_STATE_PARTIAL_MATCH << 6);

        assert(self != NULL);

        memset(self->desc->buf, val, self->desc->buf_size);

        self->index = 0;
        self->length = 0;
        self->cmd_type = CAT_CMD_TYPE_RUN;
}

static cat_status parse_prefix(struct cat_object *self)
{
        assert(self != NULL);

        if (read_cmd_char(self) == 0)
                return CAT_STATUS_OK;

        switch (self->current_char) {
        case 'T':
                prepare_parse_command(self);
                self->state = CAT_STATE_PARSE_COMMAND_CHAR;
                break;
        case '\n':
                ack_error(self);
                break;
        case '\r':
                self->cr_flag = true;
                break;
        default:
                self->state = CAT_STATE_ERROR;
                break;
        }

        return CAT_STATUS_BUSY;
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

static uint8_t convert_hex_char_to_value(const char ch)
{
        return ((ch >= '0') && (ch <= '9')) ? (uint8_t)(ch - '0') : (uint8_t)(ch - 'A' + 10U);
}

static int print_response_test(struct cat_object *self)
{
        assert(self != NULL);

        if (self->cmd->description != NULL) {
                if (print_string_to_buf(self, get_new_line_chars(self)) != 0)
                        return -1;
                if (print_string_to_buf(self, self->cmd->description) != 0)
                        return -1;
        }

        if ((self->cmd->test != NULL) && (self->cmd->test(self->cmd, self->desc->buf, &self->position, self->desc->buf_size) != 0))
                return -1;

        print_buffer(self);
        return 0;
}

static cat_status parse_command(struct cat_object *self)
{
        assert(self != NULL);

        if (read_cmd_char(self) == 0)
                return CAT_STATUS_OK;

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
                self->cr_flag = true;
                break;
        case '?':
                if (self->length == 0) {
                        self->state = CAT_STATE_ERROR;
                        break;
                }
                self->cmd_type = CAT_CMD_TYPE_READ;
                self->state = CAT_STATE_WAIT_READ_ACKNOWLEDGE;
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
        return CAT_STATUS_BUSY;
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

static cat_status update_command(struct cat_object *self)
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

        return CAT_STATUS_BUSY;
}

static cat_status wait_read_acknowledge(struct cat_object *self)
{
        assert(self != NULL);

        if (read_cmd_char(self) == 0)
                return CAT_STATUS_OK;

        switch (self->current_char) {
        case '\n':
                prepare_search_command(self);
                self->state = CAT_STATE_SEARCH_COMMAND;
                break;
        case '\r':
                self->cr_flag = true;
                break;
        default:
                self->state = CAT_STATE_ERROR;
                break;
        }
        return CAT_STATUS_BUSY;
}

static cat_status wait_test_acknowledge(struct cat_object *self)
{
        assert(self != NULL);

        if (read_cmd_char(self) == 0)
                return CAT_STATUS_OK;

        switch (self->current_char) {
        case '\n':
                self->position = 0;
                if ((self->cmd->var != NULL) && (self->cmd->var_num > 0)) {
                        self->state = CAT_STATE_FORMAT_TEST_ARGS;
                        self->index = 0;
                        self->var = &self->cmd->var[self->index];
                        break;
                }

                if (print_response_test(self) != 0) {
                        ack_error(self);
                        break;
                }

                ack_ok(self);
                break;
        case '\r':
                self->cr_flag = true;
                break;
        default:
                self->state = CAT_STATE_ERROR;
                break;
        }
        return CAT_STATUS_BUSY;
}

static cat_status search_command(struct cat_object *self)
{
        assert(self != NULL);

        uint8_t cmd_state = get_cmd_state(self, self->index);

        if (cmd_state != CAT_CMD_STATE_NOT_MATCH) {
                if (cmd_state == CAT_CMD_STATE_PARTIAL_MATCH) {
                        if (self->cmd != NULL) {
                                self->state = (self->current_char == '\n') ? CAT_STATE_COMMAND_NOT_FOUND : CAT_STATE_ERROR;
                                return CAT_STATUS_BUSY;
                        }
                        self->cmd = &self->desc->cmd[self->index];
                } else if (cmd_state == CAT_CMD_STATE_FULL_MATCH) {
                        self->cmd = &self->desc->cmd[self->index];
                        self->state = CAT_STATE_COMMAND_FOUND;
                        return CAT_STATUS_BUSY;
                }
        }

        if (++self->index >= self->desc->cmd_num) {
                if (self->cmd == NULL) {
                        self->state = (self->current_char == '\n') ? CAT_STATE_COMMAND_NOT_FOUND : CAT_STATE_ERROR;
                } else {
                        self->state = CAT_STATE_COMMAND_FOUND;
                }
        }

        return CAT_STATUS_BUSY;
}


static void start_processing_format_read_args(struct cat_object *self)
{
        self->position = 0;
        if ((self->cmd->var != NULL) && (self->cmd->var_num > 0)) {
                self->state = CAT_STATE_FORMAT_READ_ARGS;
                self->index = 0;
                self->var = &self->cmd->var[self->index];
                return;
        }
        if (self->cmd->read == NULL) {
                ack_error(self);
                return;
        }
        if (self->cmd->read(self->cmd, self->desc->buf, &self->position, self->desc->buf_size) != 0) {
                ack_error(self);
                return;
        }
        print_buffer(self);

        ack_ok(self);
}

static cat_status command_found(struct cat_object *self)
{
        assert(self != NULL);

        switch (self->cmd_type) {
        case CAT_CMD_TYPE_RUN:
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
                start_processing_format_read_args(self);
                break;
        case CAT_CMD_TYPE_WRITE:
                self->length = 0;
                self->desc->buf[0] = 0;
                self->state = CAT_STATE_PARSE_COMMAND_ARGS;
                break;
        default:
                ack_error(self);
                break;
        }

        return CAT_STATUS_BUSY;
}

static cat_status command_not_found(struct cat_object *self)
{
        assert(self != NULL);

        ack_error(self);
        return CAT_STATUS_BUSY;
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
                                val += convert_hex_char_to_value(ch);
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
                byte += convert_hex_char_to_value(ch);

                if (state != 0) {
                        if (size >= self->var->data_size)
                                return -1;
                        ((uint8_t *)(self->var->data))[size++] = byte;
                        byte = 0;
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
                        ((uint8_t *)(self->var->data))[size++] = ch;
                        break;
                case 2:
                        switch (ch) {
                        case '\\':
                                ch = '\\';
                                break;
                        case '"':
                                ch = '"';
                                break;
                        case 'n':
                                ch = '\n';
                                break;
                        default:
                                return -1;
                        }
                        if (size >= self->var->data_size)
                                return -1;
                        ((uint8_t *)(self->var->data))[size++] = ch;
                        state = 1;
                        break;
                case 3:
                        if ((ch == 0) || (ch == ',')) {
                                if (size >= self->var->data_size)
                                        return -1;
                                ((uint8_t *)(self->var->data))[size] = 0;
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
                *(int8_t *)(self->var->data) = val;
                break;
        case 2:
                if ((val < INT16_MIN) || (val > INT16_MAX))
                        return -1;
                *(int16_t *)(self->var->data) = val;
                break;
        case 4:
                if ((val < INT32_MIN) || (val > INT32_MAX))
                        return -1;
                *(int32_t *)(self->var->data) = val;
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
                *(uint8_t *)(self->var->data) = val;
                break;
        case 2:
                if (val > UINT16_MAX)
                        return -1;
                *(uint16_t *)(self->var->data) = val;
                break;
        case 4:
                if (val > UINT32_MAX)
                        return -1;
                *(uint32_t *)(self->var->data) = val;
                break;
        default:
                return -1;
        }
        self->write_size = self->var->data_size;
        return 0;
}

static cat_status parse_write_args(struct cat_object *self)
{
        int64_t val;
        cat_status stat;

        assert(self != NULL);

        switch (self->var->type) {
        case CAT_VAR_INT_DEC:
                stat = parse_int_decimal(self, &val);
                if (stat < 0) {
                        ack_error(self);
                        return CAT_STATUS_BUSY;
                }
                if (validate_int_range(self, val) != 0) {
                        ack_error(self);
                        return CAT_STATUS_BUSY;
                }
                break;
        case CAT_VAR_UINT_DEC:
                stat = parse_uint_decimal(self, (uint64_t *)&val);
                if (stat < 0) {
                        ack_error(self);
                        return CAT_STATUS_BUSY;
                }
                if (validate_uint_range(self, val) != 0) {
                        ack_error(self);
                        return CAT_STATUS_BUSY;
                }
                break;
        case CAT_VAR_NUM_HEX:
                stat = parse_num_hexadecimal(self, (uint64_t *)&val);
                if (stat < 0) {
                        ack_error(self);
                        return CAT_STATUS_BUSY;
                }
                if (validate_uint_range(self, val) != 0) {
                        ack_error(self);
                        return CAT_STATUS_BUSY;
                }
                break;
        case CAT_VAR_BUF_HEX:
                stat = parse_buffer_hexadecimal(self);
                if (stat < 0) {
                        ack_error(self);
                        return CAT_STATUS_BUSY;
                }
                break;
        case CAT_VAR_BUF_STRING:
                stat = parse_buffer_string(self);
                if (stat < 0) {
                        ack_error(self);
                        return CAT_STATUS_BUSY;
                }
                break;
        }

        if ((self->var->write != NULL) && (self->var->write(self->var, self->write_size) != 0)) {
                ack_error(self);
                return CAT_STATUS_BUSY;
        }

        if ((++self->index < self->cmd->var_num) && (stat > 0)) {
                self->var = &self->cmd->var[self->index];
                return CAT_STATUS_BUSY;
        }

        if (stat > 0) {
                ack_error(self);
                return CAT_STATUS_BUSY;
        }

        if ((self->cmd->need_all_vars != false) && (self->index != self->cmd->var_num)) {
                ack_error(self);
                return CAT_STATUS_BUSY;
        }

        if (self->cmd->write == NULL) {
                ack_ok(self);
                return CAT_STATUS_BUSY;
        }

        self->state = CAT_STATE_WRITE_LOOP;
        return CAT_STATUS_BUSY;
}

static int print_format_num(struct cat_object *self, char *fmt, uint32_t val)
{
        int written;
        size_t len;

        len = self->desc->buf_size - self->position;
        written = snprintf((char *)&self->desc->buf[self->position], len, fmt, val);

        if ((written < 0) || ((size_t)written >= len))
                return -1;

        self->position += written;
        return 0;
}

static int format_int_decimal(struct cat_object *self)
{
        int32_t val;

        assert(self != NULL);

        switch (self->var->data_size) {
        case 1:
                val = *(int8_t *)self->var->data;
                break;
        case 2:
                val = *(int16_t *)self->var->data;
                break;
        case 4:
                val = *(int32_t *)self->var->data;
                break;
        default:
                return -1;
        }

        if (print_format_num(self, "%d", val) != 0)
                return -1;

        return 0;
}

static int format_uint_decimal(struct cat_object *self)
{
        uint32_t val;

        assert(self != NULL);

        switch (self->var->data_size) {
        case 1:
                val = *(uint8_t *)self->var->data;
                break;
        case 2:
                val = *(uint16_t *)self->var->data;
                break;
        case 4:
                val = *(uint32_t *)self->var->data;
                break;
        default:
                return -1;
        }

        if (print_format_num(self, "%u", val) != 0)
                return -1;

        return 0;
}

static int format_num_hexadecimal(struct cat_object *self)
{
        uint32_t val;
        char fstr[8];

        assert(self != NULL);

        switch (self->var->data_size) {
        case 1:
                val = *(uint8_t *)self->var->data;
                strcpy(fstr, "0x%02X");
                break;
        case 2:
                val = *(uint16_t *)self->var->data;
                strcpy(fstr, "0x%04X");
                break;
        case 4:
                val = *(uint32_t *)self->var->data;
                strcpy(fstr, "0x%08X");
                break;
        default:
                return -1;
        }

        if (print_format_num(self, fstr, val) != 0)
                return -1;

        return 0;
}

static int format_buffer_hexadecimal(struct cat_object *self)
{
        size_t i;
        uint8_t *buf;

        assert(self != NULL);

        buf = self->var->data;
        for (i = 0; i < self->var->data_size; i++) {
                if (print_format_num(self, "%02X", buf[i]) != 0)
                        return -1;
        }
        return 0;
}

static int format_buffer_string(struct cat_object *self)
{
        size_t i = 0;
        char *buf;
        char ch;

        assert(self != NULL);

        if (print_string_to_buf(self, "\"") != 0)
                return -1;

        buf = self->var->data;
        for (i = 0; i < self->var->data_size; i++) {
                ch = buf[i];
                if (ch == 0)
                        break;
                if (ch == '\\') {
                        if (print_string_to_buf(self, "\\\\") != 0)
                                return -1;
                } else if (ch == '"') {
                        if (print_string_to_buf(self, "\\\"") != 0)
                                return -1;
                } else if (ch == '\n') {
                        if (print_string_to_buf(self, "\\n") != 0)
                                return -1;
                } else {
                        if (self->position >= self->desc->buf_size)
                                return -1;
                        self->desc->buf[self->position++] = ch;
                        if (self->position >= self->desc->buf_size)
                                return -1;
                        self->desc->buf[self->position++] = 0;
                }
        }

        if (print_string_to_buf(self, "\"") != 0)
                return -1;

        return 0;
}

static int format_info_type(struct cat_object *self)
{
        char var_type[8];

        assert(self != NULL);

        switch (self->var->type) {
        case CAT_VAR_INT_DEC:
                switch (self->var->data_size) {
                case 1:
                        strcpy(var_type, "INT8");
                        break;
                case 2:
                        strcpy(var_type, "INT16");
                        break;
                case 4:
                        strcpy(var_type, "INT32");
                        break;
                default:
                        return -1;
                }
                break;
        case CAT_VAR_UINT_DEC:
                switch (self->var->data_size) {
                case 1:
                        strcpy(var_type, "UINT8");
                        break;
                case 2:
                        strcpy(var_type, "UINT16");
                        break;
                case 4:
                        strcpy(var_type, "UINT32");
                        break;
                default:
                        return -1;
                }
                break;
        case CAT_VAR_NUM_HEX:
                switch (self->var->data_size) {
                case 1:
                        strcpy(var_type, "HEX8");
                        break;
                case 2:
                        strcpy(var_type, "HEX16");
                        break;
                case 4:
                        strcpy(var_type, "HEX32");
                        break;
                default:
                        return -1;
                }
                break;
                break;
        case CAT_VAR_BUF_HEX:
                strcpy(var_type, "HEXBUF");
                break;
        case CAT_VAR_BUF_STRING:
                strcpy(var_type, "STRING");
                break;
        }

        if (print_string_to_buf(self, "<") != 0)
                return -1;
        if (self->var->name != NULL) {
                if (print_string_to_buf(self, self->var->name) != 0)
                        return -1;
                if (print_string_to_buf(self, ":") != 0)
                        return -1;
        }
        if (print_string_to_buf(self, var_type) != 0)
                return -1;
        if (print_string_to_buf(self, ">") != 0)
                return -1;

        return 0;
}

static cat_status format_read_args(struct cat_object *self)
{
        cat_status stat;

        assert(self != NULL);

        if ((self->var->read != NULL) && (self->var->read(self->var) != 0)) {
                ack_error(self);
                return CAT_STATUS_BUSY;
        }

        switch (self->var->type) {
        case CAT_VAR_INT_DEC:
                stat = format_int_decimal(self);
                break;
        case CAT_VAR_UINT_DEC:
                stat = format_uint_decimal(self);
                break;
        case CAT_VAR_NUM_HEX:
                stat = format_num_hexadecimal(self);
                break;
        case CAT_VAR_BUF_HEX:
                stat = format_buffer_hexadecimal(self);
                break;
        case CAT_VAR_BUF_STRING:
                stat = format_buffer_string(self);
                break;
        }

        if (stat < 0) {
                ack_error(self);
                return CAT_STATUS_BUSY;
        }

        if (++self->index < self->cmd->var_num) {
                if (self->position >= self->desc->buf_size) {
                        ack_error(self);
                        return CAT_STATUS_BUSY;
                }
                self->desc->buf[self->position++] = ',';
                self->var = &self->cmd->var[self->index];
                return CAT_STATUS_BUSY;
        }

        if ((self->cmd->read != NULL) && (self->cmd->read(self->cmd, self->desc->buf, &self->position, self->desc->buf_size) != 0)) {
                ack_error(self);
                return CAT_STATUS_BUSY;
        }
        print_buffer(self);

        ack_ok(self);
        return CAT_STATUS_BUSY;
}

static cat_status format_test_args(struct cat_object *self)
{
        assert(self != NULL);

        if (format_info_type(self) < 0) {
                ack_error(self);
                return CAT_STATUS_BUSY;
        }

        if (++self->index < self->cmd->var_num) {
                if (self->position >= self->desc->buf_size) {
                        ack_error(self);
                        return CAT_STATUS_BUSY;
                }
                self->desc->buf[self->position++] = ',';
                self->var = &self->cmd->var[self->index];
                return CAT_STATUS_BUSY;
        }

        if (print_response_test(self) != 0) {
                ack_error(self);
                return CAT_STATUS_BUSY;
        }

        ack_ok(self);
        return CAT_STATUS_BUSY;
}

static cat_status parse_command_args(struct cat_object *self)
{
        assert(self != NULL);

        if (read_cmd_char(self) == 0)
                return CAT_STATUS_OK;

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
                self->index = 0;
                self->state = CAT_STATE_WRITE_LOOP;
                break;
        case '\r':
                self->cr_flag = true;
                break;
        default:
                if ((self->length == 0) && (self->current_char == '?')) {
                        if ((self->cmd->test != NULL) || ((self->cmd->var != NULL) && (self->cmd->var_num > 0))) {
                                self->cmd_type = CAT_CMD_TYPE_TEST;
                                self->state = CAT_STATE_WAIT_TEST_ACKNOWLEDGE;
                                break;
                        }
                }

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
        return CAT_STATUS_BUSY;
}

cat_status cat_trigger_unsolicited_read(struct cat_object *self, struct cat_command const *cmd)
{
        cat_status s;

        assert(self != NULL);
        assert(cmd != NULL);

        if ((self->mutex != NULL) && (self->mutex->lock() != 0))
                return CAT_STATUS_ERROR_MUTEX_LOCK;

        if (self->unsolicited_read_cmd == NULL) {
                self->unsolicited_read_cmd = cmd;
                s = CAT_STATUS_OK;
        } else {
                s = CAT_STATUS_ERROR_BUFFER_FULL;
        }

        if ((self->mutex != NULL) && (self->mutex->unlock() != 0))
                return CAT_STATUS_ERROR_MUTEX_UNLOCK;

        return s;
}

static cat_status check_unsolicited_buffer(struct cat_object *self)
{
        assert(self != NULL);

        if (self->unsolicited_read_cmd == NULL)
                return CAT_STATUS_OK;

        self->cmd = self->unsolicited_read_cmd;
        self->unsolicited_read_cmd = NULL;
        self->disable_ack = true;

        start_processing_format_read_args(self);

        return CAT_STATUS_BUSY;
}

static cat_status process_idle_state(struct cat_object *self)
{
        cat_status s;

        assert(self != NULL);

        s = check_unsolicited_buffer(self);
        if (s != CAT_STATUS_OK)
                return s;

        if (read_cmd_char(self) == 0)
                return CAT_STATUS_OK;

        switch (self->current_char) {
        case 'A':
                self->state = CAT_STATE_PARSE_PREFIX;
                break;
        case '\n':
        case '\r':
                break;
        default:
                self->state = CAT_STATE_ERROR;
                break;
        }

        return CAT_STATUS_BUSY;
}

static void enable_hold_state(struct cat_object *self)
{
        self->state = CAT_STATE_HOLD;
        self->hold_state_flag = true;
        self->hold_exit_status = 0;
}

static cat_status process_write_loop(struct cat_object *self)
{
        assert(self != NULL);

        switch (self->cmd->write(self->cmd, self->desc->buf, self->length, self->index)) {
        case CAT_RETURN_STATE_OK:
        case CAT_RETURN_STATE_DATA_OK:
                ack_ok(self);
                break;
        case CAT_RETURN_STATE_HOLD_EXIT_OK:
        case CAT_RETURN_STATE_HOLD_EXIT_ERROR:
        case CAT_RETURN_STATE_DATA_NEXT:
        case CAT_RETURN_STATE_NEXT:
                break;
        case CAT_RETURN_STATE_HOLD:
                enable_hold_state(self);
                break;
        case CAT_RETURN_STATE_ERROR:
        default:
                ack_error(self);
                break;
        }

        return CAT_STATUS_BUSY;
}

static cat_status process_hold_state(struct cat_object *self)
{
        cat_status s;

        assert(self != NULL);

        s = check_unsolicited_buffer(self);
        if (s != CAT_STATUS_OK)
                return s;

        if (self->hold_exit_status == 0)
                return CAT_STATUS_BUSY;

        self->hold_state_flag = false;
        if (self->hold_exit_status < 0) {
                ack_error(self);
        } else {
                ack_ok(self);
        }

        return CAT_STATUS_BUSY;
}

cat_status cat_hold_exit(struct cat_object *self, cat_status status)
{
        cat_status s;

        assert(self != NULL);

        if ((self->mutex != NULL) && (self->mutex->lock() != 0))
                return CAT_STATUS_ERROR_MUTEX_LOCK;

        if (self->hold_state_flag == false) {
                s = CAT_STATUS_ERROR_NOT_HOLD;
        } else {
                self->hold_exit_status = (status == CAT_STATUS_OK) ? 1 : -1;
                s = CAT_STATUS_OK;
        }

        if ((self->mutex != NULL) && (self->mutex->unlock() != 0))
                return CAT_STATUS_ERROR_MUTEX_UNLOCK;

        return s;
}

cat_status cat_service(struct cat_object *self)
{
        cat_status s;

        assert(self != NULL);

        if ((self->mutex != NULL) && (self->mutex->lock() != 0))
                return CAT_STATUS_ERROR_MUTEX_LOCK;

        switch (self->state) {
        case CAT_STATE_ERROR:
                s = error_state(self);
                break;
        case CAT_STATE_IDLE:
                s = process_idle_state(self);
                break;
        case CAT_STATE_PARSE_PREFIX:
                s = parse_prefix(self);
                break;
        case CAT_STATE_PARSE_COMMAND_CHAR:
                s = parse_command(self);
                break;
        case CAT_STATE_UPDATE_COMMAND_STATE:
                s = update_command(self);
                break;
        case CAT_STATE_WAIT_READ_ACKNOWLEDGE:
                s = wait_read_acknowledge(self);
                break;
        case CAT_STATE_SEARCH_COMMAND:
                s = search_command(self);
                break;
        case CAT_STATE_COMMAND_FOUND:
                s = command_found(self);
                break;
        case CAT_STATE_COMMAND_NOT_FOUND:
                s = command_not_found(self);
                break;
        case CAT_STATE_PARSE_COMMAND_ARGS:
                s = parse_command_args(self);
                break;
        case CAT_STATE_PARSE_WRITE_ARGS:
                s = parse_write_args(self);
                break;
        case CAT_STATE_FORMAT_READ_ARGS:
                s = format_read_args(self);
                break;
        case CAT_STATE_WAIT_TEST_ACKNOWLEDGE:
                s = wait_test_acknowledge(self);
                break;
        case CAT_STATE_FORMAT_TEST_ARGS:
                s = format_test_args(self);
                break;
        case CAT_STATE_WRITE_LOOP:
                s = process_write_loop(self);
                break;
        case CAT_STATE_HOLD:
                s = process_hold_state(self);
                break;
        default:
                s = CAT_STATUS_ERROR_UNKNOWN_STATE;
                break;
        }

        if (self->unsolicited_read_cmd != NULL)
                s = CAT_STATUS_BUSY;

        if ((self->mutex != NULL) && (self->mutex->unlock() != 0))
                return CAT_STATUS_ERROR_MUTEX_UNLOCK;

        return s;
}
