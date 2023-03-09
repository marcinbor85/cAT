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

#include <stdio.h>
#include <string.h>
#include <assert.h>

#define CAT_CMD_STATE_NOT_MATCH (0)
#define CAT_CMD_STATE_PARTIAL_MATCH (1U)
#define CAT_CMD_STATE_FULL_MATCH (2U)

#define CAT_WRITE_STATE_BEFORE (0)
#define CAT_WRITE_STATE_MAIN_BUFFER (1U)
#define CAT_WRITE_STATE_AFTER (2U)

static inline char* get_atcmd_buf(struct cat_object *self)
{
        return (char*)self->desc->buf;
}

static inline size_t get_atcmd_buf_size(struct cat_object *self)
{
        return (self->desc->unsolicited_buf != NULL) ? self->desc->buf_size : self->desc->buf_size >> 1;
}

static inline char* get_unsolicited_buf(struct cat_object *self)
{
        return (self->desc->unsolicited_buf != NULL) ? (char*)self->desc->unsolicited_buf : (char*)&self->desc->buf[self->desc->buf_size >> 1];
}

static inline size_t get_unsolicited_buf_size(struct cat_object *self)
{
        return (self->desc->unsolicited_buf != NULL) ? self->desc->unsolicited_buf_size : self->desc->buf_size >> 1;
}

static char to_upper(char ch)
{
        return (ch >= 'a' && ch <= 'z') ? ch - ('a' - 'A') : ch;
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
        self->cmd = NULL;
        self->cmd_type = CAT_CMD_TYPE_NONE;
}

static void unsolicited_reset_state(struct cat_object *self)
{
        assert(self != NULL);

        self->unsolicited_fsm.cmd = NULL;
        self->unsolicited_fsm.cmd_type = CAT_CMD_TYPE_NONE;
        self->unsolicited_fsm.state = CAT_UNSOLICITED_STATE_IDLE;
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

static cat_status is_hold(struct cat_object *self)
{
        return (self->hold_state_flag != false) ? CAT_STATUS_HOLD : CAT_STATUS_OK;
}

cat_status cat_is_hold(struct cat_object *self)
{
        cat_status s;

        assert(self != NULL);

        if ((self->mutex != NULL) && (self->mutex->lock() != 0))
                return CAT_STATUS_ERROR_MUTEX_LOCK;

        s = is_hold(self);

        if ((self->mutex != NULL) && (self->mutex->unlock() != 0))
                return CAT_STATUS_ERROR_MUTEX_UNLOCK;

        return s;
}

static bool is_variables_access_possible(struct cat_object *self, const struct cat_command *cmd, cat_var_access access)
{
        size_t i;
        bool ok;
        struct cat_variable const *var;
        (void)self;

        if (cmd->var == NULL)
                return false;

        ok = false;
        for (i = 0; i < cmd->var_num; i++) {
                var = &cmd->var[i];
                if ((var->access == CAT_VAR_ACCESS_READ_WRITE) || (var->access == access)) {
                        ok = true;
                        break;
                }
        }

        return ok;
}

static bool is_unsolicited_buffer_full(struct cat_object *self)
{
        assert(self != NULL);

        return (self->unsolicited_fsm.unsolicited_cmd_buffer_items_count == CAT_UNSOLICITED_CMD_BUFFER_SIZE) ? true : false;
}

static bool is_unsolicited_buffer_empty(struct cat_object *self)
{
        assert(self != NULL);

        return (self->unsolicited_fsm.unsolicited_cmd_buffer_items_count == 0) ? true : false;
}

static cat_status pop_unsolicited_cmd(struct cat_object *self, struct cat_command const **cmd, cat_cmd_type *type)
{
        struct cat_unsolicited_cmd *item;

        assert(self != NULL);
        assert(cmd != NULL);
        assert(type != NULL);

        if (is_unsolicited_buffer_empty(self) != false)
                return CAT_STATUS_ERROR_BUFFER_EMPTY;

        item = &self->unsolicited_fsm.unsolicited_cmd_buffer[self->unsolicited_fsm.unsolicited_cmd_buffer_head];

        *cmd = item->cmd;
        *type = item->type;

        if (++self->unsolicited_fsm.unsolicited_cmd_buffer_head >= CAT_UNSOLICITED_CMD_BUFFER_SIZE)
                self->unsolicited_fsm.unsolicited_cmd_buffer_head = 0;

        self->unsolicited_fsm.unsolicited_cmd_buffer_items_count--;

        return CAT_STATUS_OK;
}

static cat_status push_unsolicited_cmd(struct cat_object *self, struct cat_command const *cmd, cat_cmd_type type)
{
        struct cat_unsolicited_cmd *item;

        assert(self != NULL);
        assert(cmd != NULL);
        assert(((type == CAT_CMD_TYPE_READ) || (type == CAT_CMD_TYPE_TEST)));

        if (is_unsolicited_buffer_full(self) != false)
                return CAT_STATUS_ERROR_BUFFER_FULL;

        item = &self->unsolicited_fsm.unsolicited_cmd_buffer[self->unsolicited_fsm.unsolicited_cmd_buffer_tail];

        item->cmd = cmd;
        item->type = type;

        if (++self->unsolicited_fsm.unsolicited_cmd_buffer_tail >= CAT_UNSOLICITED_CMD_BUFFER_SIZE)
                self->unsolicited_fsm.unsolicited_cmd_buffer_tail = 0;

        self->unsolicited_fsm.unsolicited_cmd_buffer_items_count++;

        return CAT_STATUS_OK;
}

cat_status cat_is_unsolicited_buffer_full(struct cat_object *self)
{
        bool s;

        assert(self != NULL);

        if ((self->mutex != NULL) && (self->mutex->lock() != 0))
                return CAT_STATUS_ERROR_MUTEX_LOCK;

        s = is_unsolicited_buffer_full(self);

        if ((self->mutex != NULL) && (self->mutex->unlock() != 0))
                return CAT_STATUS_ERROR_MUTEX_UNLOCK;

        return (s != false) ? CAT_STATUS_ERROR_BUFFER_FULL : CAT_STATUS_OK;
}

static struct cat_command* get_command_by_fsm(struct cat_object *self, cat_fsm_type fsm)
{
        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        switch (fsm) {
        case CAT_FSM_TYPE_ATCMD:
                return (struct cat_command*)self->cmd;
        case CAT_FSM_TYPE_UNSOLICITED:
                return (struct cat_command*)self->unsolicited_fsm.cmd;
        default:
                assert(false);
        }

        return NULL;
}

struct cat_command const* cat_get_processed_command(struct cat_object *self, cat_fsm_type fsm)
{
        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        return get_command_by_fsm(self, fsm);
}

cat_status cat_is_unsolicited_event_buffered(struct cat_object *self, struct cat_command const *cmd, cat_cmd_type type)
{
        assert(self != NULL);
        assert(cmd != NULL);
        assert(type < CAT_CMD_TYPE__TOTAL_NUM);

        size_t num = self->unsolicited_fsm.unsolicited_cmd_buffer_items_count;
        size_t index = self->unsolicited_fsm.unsolicited_cmd_buffer_head;
        cat_status ret = CAT_STATUS_OK;
        struct cat_unsolicited_cmd *item;

        if ((self->unsolicited_fsm.cmd == cmd) && ((type == CAT_CMD_TYPE_NONE) || (self->unsolicited_fsm.cmd_type == type)))
                ret =  CAT_STATUS_BUSY;

        while ((num > 0) && (ret == CAT_STATUS_OK)) {
                item = &self->unsolicited_fsm.unsolicited_cmd_buffer[index];
                if ((item->cmd == cmd) && ((type == CAT_CMD_TYPE_NONE) || (item->type == type)))
                        ret = CAT_STATUS_BUSY;

                --num;
                if (++index >= CAT_UNSOLICITED_CMD_BUFFER_SIZE)
                        index = 0;
        }

        return ret;
}

static const char *get_new_line_chars(struct cat_object *self)
{
        static const char *crlf = "\r\n";
        return &crlf[(self->cr_flag != false) ? 0 : 1];
}

static void start_flush_io_buffer(struct cat_object *self, cat_state state_after)
{
        assert(self != NULL);

        self->position = 0;
        self->write_buf = get_new_line_chars(self);
        self->write_state = CAT_WRITE_STATE_BEFORE;
        self->write_state_after = state_after;
        self->state = CAT_STATE_FLUSH_IO_WRITE_WAIT;
}

static void unsolicited_start_flush_io_buffer(struct cat_object *self, cat_unsolicited_state state_after)
{
        assert(self != NULL);

        self->unsolicited_fsm.position = 0;
        self->unsolicited_fsm.write_buf = get_new_line_chars(self);
        self->unsolicited_fsm.write_state = CAT_WRITE_STATE_BEFORE;
        self->unsolicited_fsm.write_state_after = state_after;
        self->unsolicited_fsm.state = CAT_UNSOLICITED_STATE_FLUSH_IO_WRITE_WAIT;
}

static void start_flush_io_buffer_raw(struct cat_object *self, cat_state state_after)
{
        assert(self != NULL);

        self->position = 0;
        self->write_buf = get_atcmd_buf(self);
        self->write_state = CAT_WRITE_STATE_AFTER;
        self->write_state_after = state_after;
        self->state = CAT_STATE_FLUSH_IO_WRITE_WAIT;
}

static void ack_error(struct cat_object *self)
{
        assert(self != NULL);

        strncpy(get_atcmd_buf(self), "ERROR", get_atcmd_buf_size(self));
        start_flush_io_buffer(self, CAT_STATE_AFTER_FLUSH_RESET);
}

static void ack_ok(struct cat_object *self)
{
        assert(self != NULL);

        strncpy(get_atcmd_buf(self), "OK", get_atcmd_buf_size(self));
        start_flush_io_buffer(self, CAT_STATE_AFTER_FLUSH_RESET);
}

static size_t get_left_buffer_space_by_fsm(struct cat_object *self, cat_fsm_type fsm)
{
        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        switch (fsm) {
        case CAT_FSM_TYPE_ATCMD:
                return get_atcmd_buf_size(self) - self->position;
        case CAT_FSM_TYPE_UNSOLICITED:
                return get_unsolicited_buf_size(self) - self->unsolicited_fsm.position;
        default:
                assert(false);
        }

        return 0;
}

static char* get_current_buffer_by_fsm(struct cat_object *self, cat_fsm_type fsm)
{
        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        switch (fsm) {
        case CAT_FSM_TYPE_ATCMD:
                return &(get_atcmd_buf(self)[self->position]);
        case CAT_FSM_TYPE_UNSOLICITED:
                return &(get_unsolicited_buf(self)[self->unsolicited_fsm.position]);
        default:
                assert(false);
        }

        return NULL;
}

static void move_position_by_fsm(struct cat_object *self, size_t offset, cat_fsm_type fsm)
{
        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        switch (fsm) {
        case CAT_FSM_TYPE_ATCMD:
                self->position += offset;
                break;
        case CAT_FSM_TYPE_UNSOLICITED:
                self->unsolicited_fsm.position += offset;
                break;
        default:
                assert(false);
        }
}

static int print_nstring_to_buf(struct cat_object *self, const char *str, size_t len, cat_fsm_type fsm)
{
        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        if (len >= get_left_buffer_space_by_fsm(self, fsm))
                return -1;

        memcpy(get_current_buffer_by_fsm(self, fsm), str, len);
        move_position_by_fsm(self, len, fsm);
        get_current_buffer_by_fsm(self, fsm)[0] = '\0';
        return 0;
}

static int print_string_to_buf(struct cat_object *self, const char *str, cat_fsm_type fsm)
{
        return print_nstring_to_buf(self, str, strlen(str), fsm);
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

static struct cat_command const* get_command_by_index(struct cat_object *self, size_t index)
{
        size_t i, j;
        struct cat_command_group const *cmd_group;

        assert(self != NULL);
        assert(index < self->commands_num);

        j = 0;
        for (i = 0; i < self->desc->cmd_group_num; i++) {
                cmd_group = self->desc->cmd_group[i];

                if (index >= j + cmd_group->cmd_num) {
                        j += cmd_group->cmd_num;
                        continue;
                }

                return &cmd_group->cmd[index - j];
        }

        return NULL;
}

static void unsolicited_init(struct cat_object *self)
{
        self->unsolicited_fsm.unsolicited_cmd_buffer_tail = 0;
        self->unsolicited_fsm.unsolicited_cmd_buffer_head = 0;
        self->unsolicited_fsm.unsolicited_cmd_buffer_items_count = 0;

        unsolicited_reset_state(self);
}

void cat_init(struct cat_object *self, const struct cat_descriptor *desc, const struct cat_io_interface *io, const struct cat_mutex_interface *mutex)
{
        size_t i, j;
        struct cat_command_group const *cmd_group;

        assert(self != NULL);
        assert(desc != NULL);
        assert(io != NULL);

        assert(desc->cmd_group != NULL);
        assert(desc->cmd_group_num > 0);

        self->commands_num = 0;
        for (i = 0; i < desc->cmd_group_num; i++) {
                cmd_group = desc->cmd_group[i];

                assert(cmd_group->cmd != NULL);
                assert(cmd_group->cmd_num > 0);

                self->commands_num += cmd_group->cmd_num;

                for (j = 0; j < cmd_group->cmd_num; j++) {
                        assert(cmd_group->cmd[j].name != NULL);
                        if (cmd_group->cmd[j].implicit_write != false) {
                                assert(cmd_group->cmd[j].read == NULL);
                                assert(cmd_group->cmd[j].run == NULL);
                                assert(cmd_group->cmd[j].test == NULL);
                        }
                }
        }

        assert(desc->buf != NULL);
        assert(desc->buf_size * 4U >= self->commands_num);

        self->desc = desc;
        self->io = io;
        self->mutex = mutex;
        self->hold_state_flag = false;
        self->hold_exit_status = 0;
        self->implicit_write_flag = false;

        reset_state(self);

        unsolicited_init(self);
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

        memset(get_atcmd_buf(self), val, get_atcmd_buf_size(self));

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
        self->partial_cntr = 0;
        self->cmd = NULL;
}

static int is_valid_cmd_name_char(const char ch)
{
        return (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || (ch == '+') || (ch == '#') || (ch == '$') || (ch == '@') || (ch == '_') || (ch == '%') || (ch == '&');
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


static void end_processing_with_error(struct cat_object *self, cat_fsm_type fsm)
{
        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        switch (fsm) {
        case CAT_FSM_TYPE_ATCMD:
                ack_error(self);
                break;
        case CAT_FSM_TYPE_UNSOLICITED:
                unsolicited_reset_state(self);
                break;
        default:
                assert(false);
        }
}

static void end_processing_with_ok(struct cat_object *self, cat_fsm_type fsm)
{
        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        switch (fsm) {
        case CAT_FSM_TYPE_ATCMD:
                ack_ok(self);
                break;
        case CAT_FSM_TYPE_UNSOLICITED:
                unsolicited_reset_state(self);
                break;
        default:
                assert(false);
        }
}

static void reset_position(struct cat_object *self, cat_fsm_type fsm)
{
        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        switch (fsm) {
        case CAT_FSM_TYPE_ATCMD:
                self->position = 0;
                break;
        case CAT_FSM_TYPE_UNSOLICITED:
                self->unsolicited_fsm.position = 0;
                break;
        default:
                assert(false);
        }
}

static struct cat_variable* get_var_by_fsm(struct cat_object *self, cat_fsm_type fsm)
{
        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        switch (fsm) {
        case CAT_FSM_TYPE_ATCMD:
                return (struct cat_variable*)self->var;
        case CAT_FSM_TYPE_UNSOLICITED:
                return (struct cat_variable*)self->unsolicited_fsm.var;
        default:
                assert(false);
        }

        return NULL;
}

static int print_response_test(struct cat_object *self, cat_fsm_type fsm)
{
        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        struct cat_command *cmd = get_command_by_fsm(self, fsm);

        if (cmd->description != NULL) {
                if (print_string_to_buf(self, get_new_line_chars(self), fsm) != 0)
                        return -1;
                if (print_string_to_buf(self, cmd->description, fsm) != 0)
                        return -1;
        }

        if (cmd->test != NULL) {
                switch (fsm) {
                case CAT_FSM_TYPE_ATCMD:
                        self->state = CAT_STATE_TEST_LOOP;
                        break;
                case CAT_FSM_TYPE_UNSOLICITED:
                        self->unsolicited_fsm.state = CAT_UNSOLICITED_STATE_TEST_LOOP;
                        break;
                default:
                        assert(false);
                }
                return 0;
        }

        switch (fsm) {
        case CAT_FSM_TYPE_ATCMD:
                start_flush_io_buffer(self, CAT_STATE_AFTER_FLUSH_OK);
                break;
        case CAT_FSM_TYPE_UNSOLICITED:
                unsolicited_start_flush_io_buffer(self, CAT_UNSOLICITED_STATE_AFTER_FLUSH_OK);
                break;
        default:
                assert(false);
        }

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

static bool is_command_disable(struct cat_object *self, size_t index)
{
        size_t i, j;
        struct cat_command_group const *cmd_group;

        assert(self != NULL);
        assert(index < self->commands_num);

        j = 0;
        for (i = 0; i < self->desc->cmd_group_num; i++) {
                cmd_group = self->desc->cmd_group[i];

                if (index >= j + cmd_group->cmd_num) {
                        j += cmd_group->cmd_num;
                        continue;
                }

                if (cmd_group->disable != false)
                        return true;

                if (cmd_group->cmd[index - j].disable != false)
                        return true;

                break;
        }

        return false;
}

static uint8_t get_cmd_state(struct cat_object *self, size_t i)
{
        uint8_t s;

        assert(self != NULL);
        assert(i < self->commands_num);

        if (is_command_disable(self, i) != false)
                return CAT_CMD_STATE_NOT_MATCH;

        s = get_atcmd_buf(self)[i >> 2];
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

        s = get_atcmd_buf(self)[n];
        s &= ~(0x03 << k);
        s |= (state & 0x03) << k;
        get_atcmd_buf(self)[n] = s;
}

static cat_status update_command(struct cat_object *self)
{
        assert(self != NULL);

        struct cat_command const *cmd = get_command_by_index(self, self->index);
        size_t cmd_name_len;

        if (get_cmd_state(self, self->index) != CAT_CMD_STATE_NOT_MATCH) {
                cmd_name_len = strlen(cmd->name);

                if (self->length > cmd_name_len) {
                        set_cmd_state(self, self->index, CAT_CMD_STATE_NOT_MATCH);
                } else if (to_upper(cmd->name[self->length - 1]) != self->current_char) {
                        set_cmd_state(self, self->index, CAT_CMD_STATE_NOT_MATCH);
                } else if (self->length == cmd_name_len) {
                        set_cmd_state(self, self->index, CAT_CMD_STATE_FULL_MATCH);

                        if (cmd->implicit_write != false)
                                self->implicit_write_flag = true;
                }
        }

        if (++self->index >= self->commands_num) {
                self->index = 0;

                if (self->implicit_write_flag == false) {
                        self->state = CAT_STATE_PARSE_COMMAND_CHAR;
                } else {
                        self->cmd_type = CAT_CMD_TYPE_WRITE;
                        prepare_search_command(self);
                        self->state = CAT_STATE_SEARCH_COMMAND;
                        self->implicit_write_flag = false;
                }
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

static void start_processing_format_test_args(struct cat_object *self, cat_fsm_type fsm)
{
        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        reset_position(self, fsm);

        struct cat_command *cmd = get_command_by_fsm(self, fsm);

        if (print_string_to_buf(self, cmd->name, fsm) != 0) {
                end_processing_with_error(self, fsm);
                return;
        }

        if (print_string_to_buf(self, "=", fsm) != 0) {
                end_processing_with_error(self, fsm);
                return;
        }

        if ((cmd->var != NULL) && (cmd->var_num > 0)) {
                switch (fsm) {
                case CAT_FSM_TYPE_ATCMD:
                        self->state = CAT_STATE_FORMAT_TEST_ARGS;
                        self->index = 0;
                        self->var = cmd->var;
                        break;
                case CAT_FSM_TYPE_UNSOLICITED:
                        self->unsolicited_fsm.state = CAT_UNSOLICITED_STATE_FORMAT_TEST_ARGS;
                        self->unsolicited_fsm.index = 0;
                        self->unsolicited_fsm.var = cmd->var;
                        break;
                default:
                        assert(false);
                }
                return;
        }

        if (print_response_test(self, fsm) == 0)
                return;

        end_processing_with_error(self, fsm);
}

static cat_status wait_test_acknowledge(struct cat_object *self)
{
        assert(self != NULL);

        if (read_cmd_char(self) == 0)
                return CAT_STATUS_OK;

        switch (self->current_char) {
        case '\n':
                start_processing_format_test_args(self, CAT_FSM_TYPE_ATCMD);
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
                        if ((self->cmd != NULL) && ((self->index + 1) == self->commands_num)) {
                                self->state = (self->current_char == '\n') ? CAT_STATE_COMMAND_NOT_FOUND : CAT_STATE_ERROR;
                                return CAT_STATUS_BUSY;
                        }
                        self->cmd = get_command_by_index(self, self->index);
                        self->partial_cntr++;
                } else if (cmd_state == CAT_CMD_STATE_FULL_MATCH) {
                        self->cmd = get_command_by_index(self, self->index);
                        self->state = CAT_STATE_COMMAND_FOUND;
                        return CAT_STATUS_BUSY;
                }
        }

        if (++self->index >= self->commands_num) {
                if (self->cmd == NULL) {
                        self->state = (self->current_char == '\n') ? CAT_STATE_COMMAND_NOT_FOUND : CAT_STATE_ERROR;
                } else {
                        self->state = (self->partial_cntr == 1) ? CAT_STATE_COMMAND_FOUND : CAT_STATE_COMMAND_NOT_FOUND;
                }
        }

        return CAT_STATUS_BUSY;
}

static void start_processing_format_read_args(struct cat_object *self, cat_fsm_type fsm)
{
        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        reset_position(self, fsm);

        struct cat_command *cmd = get_command_by_fsm(self, fsm);

        if (print_string_to_buf(self, cmd->name, fsm) != 0) {
                end_processing_with_error(self, fsm);
                return;
        }

        if (print_string_to_buf(self, "=", fsm) != 0) {
                end_processing_with_error(self, fsm);
                return;
        }

        if (is_variables_access_possible(self, cmd, CAT_VAR_ACCESS_READ_ONLY) != false) {
                switch (fsm) {
                case CAT_FSM_TYPE_ATCMD:
                        self->state = CAT_STATE_FORMAT_READ_ARGS;
                        self->index = 0;
                        self->var = cmd->var;
                        break;
                case CAT_FSM_TYPE_UNSOLICITED:
                        self->unsolicited_fsm.state = CAT_UNSOLICITED_STATE_FORMAT_READ_ARGS;
                        self->unsolicited_fsm.index = 0;
                        self->unsolicited_fsm.var = cmd->var;
                        break;
                default:
                        assert(false);
                }

                return;
        }
        if (cmd->read == NULL) {
                end_processing_with_error(self, fsm);
                return;
        }

        switch (fsm) {
        case CAT_FSM_TYPE_ATCMD:
                self->state = CAT_STATE_READ_LOOP;
                break;
        case CAT_FSM_TYPE_UNSOLICITED:
                self->unsolicited_fsm.state = CAT_UNSOLICITED_STATE_READ_LOOP;
                break;
        default:
                assert(false);
        }
}

static cat_status command_found(struct cat_object *self)
{
        assert(self != NULL);

        switch (self->cmd_type) {
        case CAT_CMD_TYPE_RUN:
                if (self->cmd->only_test != false) {
                        ack_error(self);
                        break;
                }
                if (self->cmd->run == NULL) {
                        ack_error(self);
                        break;
                }

                self->state = CAT_STATE_RUN_LOOP;
                break;
        case CAT_CMD_TYPE_READ:
                if (self->cmd->only_test != false) {
                        ack_error(self);
                        break;
                }
                start_processing_format_read_args(self, CAT_FSM_TYPE_ATCMD);
                break;
        case CAT_CMD_TYPE_WRITE:
                self->length = 0;
                get_atcmd_buf(self)[0] = 0;
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
                ch = get_atcmd_buf(self)[self->position++];

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
                ch = get_atcmd_buf(self)[self->position++];

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
                ch = get_atcmd_buf(self)[self->position++];
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
                ch = get_atcmd_buf(self)[self->position++];
                ch = to_upper(ch);

                if ((size > 0) && (state == 0) && ((ch == 0) || (ch == ','))) {
                        if (self->var->access == CAT_VAR_ACCESS_READ_ONLY) {
                                self->write_size = 0;
                        } else {
                                self->write_size = size;
                        }
                        return (ch == ',') ? 1 : 0;
                }

                if (is_valid_hex_char(ch) == 0)
                        return -1;

                byte <<= 4;
                byte += convert_hex_char_to_value(ch);

                if (state != 0) {
                        if (size >= self->var->data_size)
                                return -1;
                        if (self->var->access == CAT_VAR_ACCESS_READ_ONLY) {
                                size++;
                        } else {
                                ((uint8_t *)(self->var->data))[size++] = byte;
                        }
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
                ch = get_atcmd_buf(self)[self->position++];

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
                        if (self->var->access == CAT_VAR_ACCESS_READ_ONLY) {
                                size++;
                        } else {
                                ((uint8_t *)(self->var->data))[size++] = ch;
                        }
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
                        if (self->var->access == CAT_VAR_ACCESS_READ_ONLY) {
                                size++;
                        } else {
                                ((uint8_t *)(self->var->data))[size++] = ch;
                        }
                        state = 1;
                        break;
                case 3:
                        if ((ch == 0) || (ch == ',')) {
                                if (size >= self->var->data_size)
                                        return -1;
                                if (self->var->access == CAT_VAR_ACCESS_READ_ONLY) {
                                        self->write_size = 0;
                                } else {
                                        ((uint8_t *)(self->var->data))[size] = 0;
                                        self->write_size = size;
                                }
                                return (ch == ',') ? 1 : 0;
                        } else {
                                return -1;
                        }
                        break;
                default:
                        break;
                }
        }

        return -1;
}

static int validate_int_range(struct cat_object *self, int64_t val)
{
        if (self->var->access == CAT_VAR_ACCESS_READ_ONLY) {
                self->write_size = 0;
                return 0;
        }

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
        if (self->var->access == CAT_VAR_ACCESS_READ_ONLY) {
                self->write_size = 0;
                return 0;
        }

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
        default:
                return CAT_STATUS_ERROR;
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

static int print_format_num(struct cat_object *self, char *fmt, uint32_t val, cat_fsm_type fsm)
{
        int written;
        size_t len;

        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        len = get_left_buffer_space_by_fsm(self, fsm);
        written = snprintf(get_current_buffer_by_fsm(self, fsm), len, fmt, val);

        if ((written < 0) || ((size_t)written >= len))
                return -1;

        move_position_by_fsm(self, written, fsm);
        return 0;
}

static int format_int_decimal(struct cat_object *self, cat_fsm_type fsm)
{
        int32_t val;

        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        struct cat_variable *var = get_var_by_fsm(self, fsm);

        switch (var->data_size) {
        case 1:
                val = *(int8_t *)var->data;
                break;
        case 2:
                val = *(int16_t *)var->data;
                break;
        case 4:
                val = *(int32_t *)var->data;
                break;
        default:
                return -1;
        }

        if (var->access == CAT_VAR_ACCESS_WRITE_ONLY)
                val = 0;

        if (print_format_num(self, "%d", val, fsm) != 0)
                return -1;

        return 0;
}

static int format_uint_decimal(struct cat_object *self, cat_fsm_type fsm)
{
        uint32_t val;

        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        struct cat_variable *var = get_var_by_fsm(self, fsm);

        switch (var->data_size) {
        case 1:
                val = *(uint8_t *)var->data;
                break;
        case 2:
                val = *(uint16_t *)var->data;
                break;
        case 4:
                val = *(uint32_t *)var->data;
                break;
        default:
                return -1;
        }

        if (var->access == CAT_VAR_ACCESS_WRITE_ONLY)
                val = 0;

        if (print_format_num(self, "%u", val, fsm) != 0)
                return -1;

        return 0;
}

static int format_num_hexadecimal(struct cat_object *self, cat_fsm_type fsm)
{
        uint32_t val;
        char fstr[8];

        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        struct cat_variable *var = get_var_by_fsm(self, fsm);

        switch (var->data_size) {
        case 1:
                val = *(uint8_t *)var->data;
                strcpy(fstr, "0x%02X");
                break;
        case 2:
                val = *(uint16_t *)var->data;
                strcpy(fstr, "0x%04X");
                break;
        case 4:
                val = *(uint32_t *)var->data;
                strcpy(fstr, "0x%08X");
                break;
        default:
                return -1;
        }

        if (var->access == CAT_VAR_ACCESS_WRITE_ONLY)
                val = 0;

        if (print_format_num(self, fstr, val, fsm) != 0)
                return -1;

        return 0;
}

static int format_buffer_hexadecimal(struct cat_object *self, cat_fsm_type fsm)
{
        size_t i;
        uint8_t *buf;
        uint8_t val;

        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        struct cat_variable *var = get_var_by_fsm(self, fsm);

        buf = var->data;
        for (i = 0; i < var->data_size; i++) {
                if (var->access == CAT_VAR_ACCESS_WRITE_ONLY) {
                        val = 0;
                } else {
                        val = buf[i];
                }

                if (print_format_num(self, "%02X", val, fsm) != 0)
                        return -1;
        }
        return 0;
}

static int format_buffer_string(struct cat_object *self, cat_fsm_type fsm)
{
        size_t i = 0;
        char *buf;
        size_t buf_size;
        char ch;

        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        struct cat_variable *var = get_var_by_fsm(self, fsm);

        if (var->access == CAT_VAR_ACCESS_WRITE_ONLY) {
                buf_size = 0;
        } else {
                buf_size = var->data_size;
        }

        if (print_string_to_buf(self, "\"", fsm) != 0)
                return -1;

        buf = var->data;
        for (i = 0; i < buf_size; i++) {
                ch = buf[i];
                if (ch == 0)
                        break;
                if (ch == '\\') {
                        if (print_string_to_buf(self, "\\\\", fsm) != 0)
                                return -1;
                } else if (ch == '"') {
                        if (print_string_to_buf(self, "\\\"", fsm) != 0)
                                return -1;
                } else if (ch == '\n') {
                        if (print_string_to_buf(self, "\\n", fsm) != 0)
                                return -1;
                } else {
                        if (print_nstring_to_buf(self, &ch, 1, fsm) != 0)
                                return -1;
                }
        }

        if (print_string_to_buf(self, "\"", fsm) != 0)
                return -1;

        return 0;
}

static int format_info_type(struct cat_object *self, cat_fsm_type fsm)
{
        char var_type[8];
        char accessor[8];

        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        struct cat_variable *var = get_var_by_fsm(self, fsm);

        switch (var->access) {
        case CAT_VAR_ACCESS_READ_WRITE:
                strcpy(accessor, "RW");
                break;
        case CAT_VAR_ACCESS_READ_ONLY:
                strcpy(accessor, "RO");
                break;
        case CAT_VAR_ACCESS_WRITE_ONLY:
                strcpy(accessor, "WO");
                break;
        default:
                strcpy(accessor, "??");
                break;
        }

        switch (var->type) {
        case CAT_VAR_INT_DEC:
                switch (var->data_size) {
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
                switch (var->data_size) {
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
                switch (var->data_size) {
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
        case CAT_VAR_BUF_HEX:
                strcpy(var_type, "HEXBUF");
                break;
        case CAT_VAR_BUF_STRING:
                strcpy(var_type, "STRING");
                break;
        default:
                return -1;
        }

        if (print_string_to_buf(self, "<", fsm) != 0)
                return -1;
        if (var->name != NULL) {
                if (print_string_to_buf(self, var->name, fsm) != 0)
                        return -1;
                if (print_string_to_buf(self, ":", fsm) != 0)
                        return -1;
        }
        if (print_string_to_buf(self, var_type, fsm) != 0)
                return -1;
        if (print_string_to_buf(self, "[", fsm) != 0)
                return -1;
        if (print_string_to_buf(self, accessor, fsm) != 0)
                return -1;
        if (print_string_to_buf(self, "]", fsm) != 0)
                return -1;
        if (print_string_to_buf(self, ">", fsm) != 0)
                return -1;

        return 0;
}

static cat_status next_format_var_by_fsm(struct cat_object *self, cat_fsm_type fsm)
{
        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        struct cat_command *cmd = get_command_by_fsm(self, fsm);

        switch (fsm) {
        case CAT_FSM_TYPE_ATCMD:
                if (++self->index < cmd->var_num) {
                        if (self->position >= get_atcmd_buf_size(self)) {
                                end_processing_with_error(self, fsm);
                                return CAT_STATUS_BUSY;
                        }
                        get_atcmd_buf(self)[self->position++] = ',';
                        self->var = &cmd->var[self->index];
                        return CAT_STATUS_BUSY;
                }
                break;
        case CAT_FSM_TYPE_UNSOLICITED:
                if (++self->unsolicited_fsm.index < cmd->var_num) {
                        if (self->unsolicited_fsm.position >= get_unsolicited_buf_size(self)) {
                                end_processing_with_error(self, fsm);
                                return CAT_STATUS_BUSY;
                        }
                        get_unsolicited_buf(self)[self->unsolicited_fsm.position++] = ',';
                        self->unsolicited_fsm.var = &cmd->var[self->unsolicited_fsm.index];
                        return CAT_STATUS_BUSY;
                }
                break;
        default:
                assert(false);
        }

        return CAT_STATUS_OK;
}

static cat_status format_read_args(struct cat_object *self, cat_fsm_type fsm)
{
        cat_status stat;

        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        struct cat_variable *var = get_var_by_fsm(self, fsm);

        if ((var->read != NULL) && (var->read(var) != 0)) {
                end_processing_with_error(self, fsm);
                return CAT_STATUS_BUSY;
        }

        switch (var->type) {
        case CAT_VAR_INT_DEC:
                stat = format_int_decimal(self, fsm);
                break;
        case CAT_VAR_UINT_DEC:
                stat = format_uint_decimal(self, fsm);
                break;
        case CAT_VAR_NUM_HEX:
                stat = format_num_hexadecimal(self, fsm);
                break;
        case CAT_VAR_BUF_HEX:
                stat = format_buffer_hexadecimal(self, fsm);
                break;
        case CAT_VAR_BUF_STRING:
                stat = format_buffer_string(self, fsm);
                break;
        default:
                return CAT_STATUS_ERROR;
        }

        if (stat < 0) {
                end_processing_with_error(self, fsm);
                return CAT_STATUS_BUSY;
        }

        stat = next_format_var_by_fsm(self, fsm);
        if (stat != CAT_STATUS_OK)
                return stat;

        struct cat_command *cmd = get_command_by_fsm(self, fsm);

        if (cmd->read != NULL) {
                switch (fsm) {
                case CAT_FSM_TYPE_ATCMD:
                        self->state = CAT_STATE_READ_LOOP;
                        break;
                case CAT_FSM_TYPE_UNSOLICITED:
                        self->unsolicited_fsm.state = CAT_UNSOLICITED_STATE_READ_LOOP;
                        break;
                default:
                        assert(false);
                }

                return CAT_STATUS_BUSY;
        }

        switch (fsm) {
        case CAT_FSM_TYPE_ATCMD:
                start_flush_io_buffer(self, CAT_STATE_AFTER_FLUSH_OK);
                break;
        case CAT_FSM_TYPE_UNSOLICITED:
                unsolicited_start_flush_io_buffer(self, CAT_UNSOLICITED_STATE_AFTER_FLUSH_OK);
                break;
        default:
                assert(false);
        }

        return CAT_STATUS_BUSY;
}

static cat_status format_test_args(struct cat_object *self, cat_fsm_type fsm)
{
        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        if (format_info_type(self, fsm) < 0) {
                end_processing_with_error(self, fsm);
                return CAT_STATUS_BUSY;
        }

        cat_status stat = next_format_var_by_fsm(self, fsm);
        if (stat != CAT_STATUS_OK)
                return stat;

        if (print_response_test(self, fsm) == 0)
                return CAT_STATUS_BUSY;

        end_processing_with_error(self, fsm);
        return CAT_STATUS_BUSY;
}

static cat_status parse_command_args(struct cat_object *self)
{
        assert(self != NULL);

        if (read_cmd_char(self) == 0)
                return CAT_STATUS_OK;

        switch (self->current_char) {
        case '\n':
                if (self->cmd->only_test != false) {
                        ack_error(self);
                        break;
                }
                if (is_variables_access_possible(self, self->cmd, CAT_VAR_ACCESS_WRITE_ONLY) != false) {
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
                        if (((self->cmd->test != NULL) || ((self->cmd->var != NULL) && (self->cmd->var_num > 0))) && (self->cmd->implicit_write == false)) {
                                self->cmd_type = CAT_CMD_TYPE_TEST;
                                self->state = CAT_STATE_WAIT_TEST_ACKNOWLEDGE;
                                break;
                        }
                }

                if (self->length >= get_atcmd_buf_size(self)) {
                        self->state = CAT_STATE_ERROR;
                        break;
                }
                get_atcmd_buf(self)[self->length++] = self->current_char;
                if (self->length < get_atcmd_buf_size(self)) {
                        get_atcmd_buf(self)[self->length] = 0;
                } else {
                        self->state = CAT_STATE_ERROR;
                }
                break;
        }
        return CAT_STATUS_BUSY;
}

cat_status cat_trigger_unsolicited_event(struct cat_object *self, struct cat_command const *cmd, cat_cmd_type type)
{
        cat_status s;

        assert(self != NULL);
        assert(cmd != NULL);
        assert(((type == CAT_CMD_TYPE_READ) || (type == CAT_CMD_TYPE_TEST)));

        if ((self->mutex != NULL) && (self->mutex->lock() != 0))
                return CAT_STATUS_ERROR_MUTEX_LOCK;

        s = push_unsolicited_cmd(self, cmd, type);

        if ((self->mutex != NULL) && (self->mutex->unlock() != 0))
                return CAT_STATUS_ERROR_MUTEX_UNLOCK;

        return s;
}

cat_status cat_trigger_unsolicited_read(struct cat_object *self, struct cat_command const *cmd)
{
        return cat_trigger_unsolicited_event(self, cmd, CAT_CMD_TYPE_READ);
}

cat_status cat_trigger_unsolicited_test(struct cat_object *self, struct cat_command const *cmd)
{
        return cat_trigger_unsolicited_event(self, cmd, CAT_CMD_TYPE_TEST);
}

static void check_unsolicited_buffers(struct cat_object *self)
{
        cat_cmd_type type;

        assert(self != NULL);

        if (pop_unsolicited_cmd(self, &self->unsolicited_fsm.cmd, &type) != CAT_STATUS_OK)
                return;

        self->unsolicited_fsm.cmd_type = type;

        switch (type) {
        case CAT_CMD_TYPE_READ:
                start_processing_format_read_args(self, CAT_FSM_TYPE_UNSOLICITED);
                break;
        case CAT_CMD_TYPE_TEST:
                start_processing_format_test_args(self, CAT_FSM_TYPE_UNSOLICITED);
                break;
        default:
                break;
        }
}

static cat_status process_idle_state(struct cat_object *self)
{
        assert(self != NULL);

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
        assert(self != NULL);

        self->state = CAT_STATE_HOLD;
        self->hold_state_flag = true;
        self->hold_exit_status = 0;
}

static cat_status hold_exit(struct cat_object *self, cat_status status)
{
        cat_status s;

        assert(self != NULL);

        if (self->hold_state_flag == false) {
                s = CAT_STATUS_ERROR_NOT_HOLD;
        } else {
                self->hold_exit_status = (status == CAT_STATUS_OK) ? 1 : -1;
                s = CAT_STATUS_OK;
        }

        return s;
}

static void start_print_cmd_list(struct cat_object *self)
{
        assert(self != NULL);

        if (self->commands_num == 0) {
                ack_ok(self);
                return;
        }

        self->index = 0;
        self->length = 0;
        self->cmd_type = CAT_CMD_TYPE_NONE;
        self->state = CAT_STATE_PRINT_CMD;
}

static bool cmd_list_next_cmd(struct cat_object *self)
{
        if (++self->index >= self->commands_num)
                return false;

        self->length = 0;
        self->cmd_type = CAT_CMD_TYPE_NONE;
        self->state = CAT_STATE_PRINT_CMD;

        return true;
}

static int print_current_cmd_full_name(struct cat_object *self, const char *suffix)
{
        if (self->length == 0) {
                if (print_string_to_buf(self, get_new_line_chars(self), CAT_FSM_TYPE_ATCMD) != 0)
                        return -1;
                self->length = 1;
        }

        if (print_string_to_buf(self, "AT", CAT_FSM_TYPE_ATCMD) != 0)
                return -1;
        if (print_string_to_buf(self, self->cmd->name, CAT_FSM_TYPE_ATCMD) != 0)
                return -1;
        if (print_string_to_buf(self, suffix, CAT_FSM_TYPE_ATCMD) != 0)
                return -1;
        if (print_string_to_buf(self, get_new_line_chars(self), CAT_FSM_TYPE_ATCMD) != 0)
                return -1;

        return 0;
}

static void print_cmd_list(struct cat_object *self)
{
        self->cmd = get_command_by_index(self, self->index);

        switch (self->cmd_type) {
        case CAT_CMD_TYPE_NONE:
                if (self->cmd->disable != false) {
                        if (cmd_list_next_cmd(self) == false)
                                ack_ok(self);
                        break;
                }

                self->cmd_type = (self->cmd->only_test != false) ? CAT_CMD_TYPE_TEST : CAT_CMD_TYPE_RUN;
                break;
        case CAT_CMD_TYPE_RUN:
                if (self->cmd->run != NULL) {
                        self->position = 0;
                        if (print_current_cmd_full_name(self, "") != 0) {
                                ack_error(self);
                                break;
                        }
                        start_flush_io_buffer_raw(self, CAT_STATE_PRINT_CMD);
                }
                self->cmd_type = CAT_CMD_TYPE_READ;
                break;
        case CAT_CMD_TYPE_READ:
                if ((self->cmd->read != NULL) || (is_variables_access_possible(self, self->cmd, CAT_VAR_ACCESS_READ_ONLY) != false)) {
                        self->position = 0;
                        if (print_current_cmd_full_name(self, "?") != 0) {
                                ack_error(self);
                                break;
                        }
                        start_flush_io_buffer_raw(self, CAT_STATE_PRINT_CMD);
                }
                self->cmd_type = CAT_CMD_TYPE_WRITE;
                break;
        case CAT_CMD_TYPE_WRITE:
                if (self->cmd->write != NULL || (is_variables_access_possible(self, self->cmd, CAT_VAR_ACCESS_WRITE_ONLY) != false)) {
                        self->position = 0;
                        if (print_current_cmd_full_name(self, "=") != 0) {
                                ack_error(self);
                                break;
                        }
                        start_flush_io_buffer_raw(self, CAT_STATE_PRINT_CMD);
                }
                self->cmd_type = CAT_CMD_TYPE_TEST;
                break;
        case CAT_CMD_TYPE_TEST:
                if (self->cmd->test != NULL || ((self->cmd->var != NULL) && (self->cmd->var_num > 0))) {
                        self->position = 0;
                        if (print_current_cmd_full_name(self, "=?") != 0) {
                                ack_error(self);
                                break;
                        }
                        start_flush_io_buffer_raw(self, CAT_STATE_PRINT_CMD);
                }
                self->cmd_type = CAT_CMD_TYPE__TOTAL_NUM;
                break;
        case CAT_CMD_TYPE__TOTAL_NUM:
                if (cmd_list_next_cmd(self) == false)
                        ack_ok(self);
                break;
        default:
                ack_error(self);
                break;
        }
}

static cat_status process_write_loop(struct cat_object *self)
{
        assert(self != NULL);

        switch (self->cmd->write(self->cmd, (uint8_t*)get_atcmd_buf(self), self->length, self->index)) {
        case CAT_RETURN_STATE_OK:
        case CAT_RETURN_STATE_DATA_OK:
                ack_ok(self);
                break;
        case CAT_RETURN_STATE_DATA_NEXT:
        case CAT_RETURN_STATE_NEXT:
                break;
        case CAT_RETURN_STATE_HOLD:
                enable_hold_state(self);
                break;
        case CAT_RETURN_STATE_HOLD_EXIT_OK:
        case CAT_RETURN_STATE_HOLD_EXIT_ERROR:
        case CAT_RETURN_STATE_ERROR:
        case CAT_RETURN_STATE_PRINT_CMD_LIST_OK:
        default:
                ack_error(self);
                break;
        }

        return CAT_STATUS_BUSY;
}

static cat_status process_run_loop(struct cat_object *self)
{
        assert(self != NULL);

        switch (self->cmd->run(self->cmd)) {
        case CAT_RETURN_STATE_OK:
        case CAT_RETURN_STATE_DATA_OK:
                ack_ok(self);
                break;
        case CAT_RETURN_STATE_DATA_NEXT:
        case CAT_RETURN_STATE_NEXT:
                break;
        case CAT_RETURN_STATE_HOLD:
                enable_hold_state(self);
                break;
        case CAT_RETURN_STATE_PRINT_CMD_LIST_OK:
                start_print_cmd_list(self);
                break;
        case CAT_RETURN_STATE_HOLD_EXIT_OK:
        case CAT_RETURN_STATE_HOLD_EXIT_ERROR:
        case CAT_RETURN_STATE_ERROR:
        default:
                ack_error(self);
                break;
        }

        return CAT_STATUS_BUSY;
}

static cat_return_state call_cmd_read_by_fsm(struct cat_object *self, cat_fsm_type fsm)
{
        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        struct cat_command *cmd = get_command_by_fsm(self, fsm);

        switch (fsm) {
        case CAT_FSM_TYPE_ATCMD:
                return cmd->read(cmd, (uint8_t*)get_atcmd_buf(self), &self->position, get_atcmd_buf_size(self));
        case CAT_FSM_TYPE_UNSOLICITED:
                return cmd->read(cmd, (uint8_t*)get_unsolicited_buf(self), &self->unsolicited_fsm.position, get_unsolicited_buf_size(self));
        default:
                assert(false);
        }
        return CAT_RETURN_STATE_ERROR;
}

static cat_status process_read_loop(struct cat_object *self, cat_fsm_type fsm)
{
        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        switch (call_cmd_read_by_fsm(self, fsm)) {
        case CAT_RETURN_STATE_OK:
                end_processing_with_ok(self, fsm);
                break;
        case CAT_RETURN_STATE_DATA_OK:
                switch (fsm) {
                case CAT_FSM_TYPE_ATCMD:
                        start_flush_io_buffer(self, CAT_STATE_AFTER_FLUSH_OK);
                        break;
                case CAT_FSM_TYPE_UNSOLICITED:
                        unsolicited_start_flush_io_buffer(self, CAT_UNSOLICITED_STATE_AFTER_FLUSH_OK);
                        break;
                default:
                        assert(false);
                }
                break;
        case CAT_RETURN_STATE_DATA_NEXT:
                switch (fsm) {
                case CAT_FSM_TYPE_ATCMD:
                        start_flush_io_buffer(self, CAT_STATE_AFTER_FLUSH_FORMAT_READ_ARGS);
                        break;
                case CAT_FSM_TYPE_UNSOLICITED:
                        unsolicited_start_flush_io_buffer(self, CAT_UNSOLICITED_STATE_AFTER_FLUSH_FORMAT_READ_ARGS);
                        break;
                default:
                        assert(false);
                }
                break;
        case CAT_RETURN_STATE_NEXT:
                start_processing_format_read_args(self, fsm);
                break;
        case CAT_RETURN_STATE_HOLD:
                enable_hold_state(self);
                break;
        case CAT_RETURN_STATE_HOLD_EXIT_OK:
                hold_exit(self, CAT_STATUS_OK);
                end_processing_with_ok(self, fsm);
                break;
        case CAT_RETURN_STATE_HOLD_EXIT_ERROR:
                hold_exit(self, CAT_STATUS_ERROR);
                end_processing_with_error(self, fsm);
                break;
        case CAT_RETURN_STATE_PRINT_CMD_LIST_OK:
        case CAT_RETURN_STATE_ERROR:
        default:
                end_processing_with_error(self, fsm);
                break;
        }

        return CAT_STATUS_BUSY;
}

static cat_return_state call_cmd_test_by_fsm(struct cat_object *self, cat_fsm_type fsm)
{
        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        struct cat_command *cmd = get_command_by_fsm(self, fsm);

        switch (fsm) {
        case CAT_FSM_TYPE_ATCMD:
                return cmd->test(cmd, (uint8_t*)get_atcmd_buf(self), &self->position, get_atcmd_buf_size(self));
        case CAT_FSM_TYPE_UNSOLICITED:
                return cmd->test(cmd, (uint8_t*)get_unsolicited_buf(self), &self->unsolicited_fsm.position, get_unsolicited_buf_size(self));
        default:
                assert(false);
        }
        return CAT_RETURN_STATE_ERROR;
}

static cat_status process_test_loop(struct cat_object *self, cat_fsm_type fsm)
{
        assert(self != NULL);
        assert(fsm < CAT_FSM_TYPE__TOTAL_NUM);

        switch (call_cmd_test_by_fsm(self, fsm)) {
        case CAT_RETURN_STATE_OK:
                end_processing_with_ok(self, fsm);
                break;
        case CAT_RETURN_STATE_DATA_OK:
                switch (fsm) {
                case CAT_FSM_TYPE_ATCMD:
                        start_flush_io_buffer(self, CAT_STATE_AFTER_FLUSH_OK);
                        break;
                case CAT_FSM_TYPE_UNSOLICITED:
                        unsolicited_start_flush_io_buffer(self, CAT_UNSOLICITED_STATE_AFTER_FLUSH_OK);
                        break;
                default:
                        assert(false);
                }
                break;
        case CAT_RETURN_STATE_DATA_NEXT:
                switch (fsm) {
                case CAT_FSM_TYPE_ATCMD:
                        start_flush_io_buffer(self, CAT_STATE_AFTER_FLUSH_FORMAT_TEST_ARGS);
                        break;
                case CAT_FSM_TYPE_UNSOLICITED:
                        unsolicited_start_flush_io_buffer(self, CAT_UNSOLICITED_STATE_AFTER_FLUSH_FORMAT_TEST_ARGS);
                        break;
                default:
                        assert(false);
                }
                break;
        case CAT_RETURN_STATE_NEXT:
                start_processing_format_test_args(self, fsm);
                break;
        case CAT_RETURN_STATE_HOLD:
                enable_hold_state(self);
                break;
        case CAT_RETURN_STATE_HOLD_EXIT_OK:
                hold_exit(self, CAT_STATUS_OK);
                end_processing_with_ok(self, fsm);
                break;
        case CAT_RETURN_STATE_HOLD_EXIT_ERROR:
                hold_exit(self, CAT_STATUS_ERROR);
                end_processing_with_error(self, fsm);
                break;
        case CAT_RETURN_STATE_PRINT_CMD_LIST_OK:
                if (fsm == CAT_FSM_TYPE_ATCMD) {
                        start_print_cmd_list(self);
                } else {
                        end_processing_with_ok(self, fsm);
                }
                break;
        case CAT_RETURN_STATE_ERROR:
        default:
                end_processing_with_error(self, fsm);
                break;
        }

        return CAT_STATUS_BUSY;
}

static cat_status process_hold_state(struct cat_object *self)
{
        assert(self != NULL);

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

        s = hold_exit(self, status);

        if ((self->mutex != NULL) && (self->mutex->unlock() != 0))
                return CAT_STATUS_ERROR_MUTEX_UNLOCK;

        return s;
}

struct cat_command const* cat_search_command_by_name(struct cat_object *self, const char *name)
{
        size_t i;
        struct cat_command const *cmd;

        assert(self != NULL);
        assert(name != NULL);

        for (i = 0; i < self->commands_num; i++) {
                cmd = get_command_by_index(self, i);
                if (strcmp(cmd->name, name) == 0)
                        return cmd;
        }

        return NULL;
}

struct cat_command_group const* cat_search_command_group_by_name(struct cat_object *self, const char *name)
{
        size_t i;
        struct cat_command_group const *cmd_group;

        assert(self != NULL);
        assert(name != NULL);

        for (i = 0; i < self->desc->cmd_group_num; i++) {
                cmd_group = self->desc->cmd_group[i];
                if ((cmd_group->name != NULL) && (strcmp(cmd_group->name, name) == 0))
                        return cmd_group;
        }

        return NULL;
}

struct cat_variable const* cat_search_variable_by_name(struct cat_object *self, struct cat_command const *cmd, const char *name)
{
        size_t i;
        struct cat_variable const *var;
        (void)self;

        assert(self != NULL);
        assert(cmd != NULL);
        assert(name != NULL);

        for (i = 0; i < cmd->var_num; i++) {
                var = &cmd->var[i];
                if ((var->name != NULL) && (strcmp(var->name, name) == 0))
                        return var;
        }

        return NULL;
}

static cat_status process_io_write_wait(struct cat_object *self)
{
        if (self->unsolicited_fsm.state != CAT_UNSOLICITED_STATE_FLUSH_IO_WRITE)
                self->state = CAT_STATE_FLUSH_IO_WRITE;

        return CAT_STATUS_BUSY;
}

static cat_status unsolicited_process_io_write_wait(struct cat_object *self)
{
        if (self->state != CAT_STATE_FLUSH_IO_WRITE)
                self->unsolicited_fsm.state = CAT_UNSOLICITED_STATE_FLUSH_IO_WRITE;

        return CAT_STATUS_BUSY;
}

static cat_status process_io_write(struct cat_object *self)
{
        char ch = self->write_buf[self->position];

        if (ch == '\0') {
                switch (self->write_state) {
                case CAT_WRITE_STATE_BEFORE:
                        self->position = 0;
                        self->write_buf = get_atcmd_buf(self);
                        self->write_state = CAT_WRITE_STATE_MAIN_BUFFER;
                        break;
                case CAT_WRITE_STATE_MAIN_BUFFER:
                        self->position = 0;
                        self->write_buf = get_new_line_chars(self);
                        self->write_state = CAT_WRITE_STATE_AFTER;
                        break;
                case CAT_WRITE_STATE_AFTER:
                        self->state = self->write_state_after;
                        break;
                default:
                        break;
                }
                return CAT_STATUS_BUSY;
        }

        if (self->io->write(ch) != 1)
                return CAT_STATUS_BUSY;

        self->position++;
        return CAT_STATUS_BUSY;
}

static cat_status unsolicited_process_io_write(struct cat_object *self)
{
        char ch = self->unsolicited_fsm.write_buf[self->unsolicited_fsm.position];

        if (ch == '\0') {
                switch (self->unsolicited_fsm.write_state) {
                case CAT_WRITE_STATE_BEFORE:
                        self->unsolicited_fsm.position = 0;
                        self->unsolicited_fsm.write_buf = get_unsolicited_buf(self);
                        self->unsolicited_fsm.write_state = CAT_WRITE_STATE_MAIN_BUFFER;
                        break;
                case CAT_WRITE_STATE_MAIN_BUFFER:
                        self->unsolicited_fsm.position = 0;
                        self->unsolicited_fsm.write_buf = get_new_line_chars(self);
                        self->unsolicited_fsm.write_state = CAT_WRITE_STATE_AFTER;
                        break;
                case CAT_WRITE_STATE_AFTER:
                        self->unsolicited_fsm.state = self->unsolicited_fsm.write_state_after;
                        break;
                }
                return CAT_STATUS_BUSY;
        }

        if (self->io->write(ch) != 1)
                return CAT_STATUS_BUSY;

        self->unsolicited_fsm.position++;
        return CAT_STATUS_BUSY;
}

static cat_status unsolicited_events_service(struct cat_object *self)
{
        cat_status s = CAT_STATUS_OK;

        switch (self->unsolicited_fsm.state) {
        case CAT_UNSOLICITED_STATE_IDLE:
                check_unsolicited_buffers(self);
                break;
        case CAT_UNSOLICITED_STATE_FORMAT_READ_ARGS:
                s = format_read_args(self, CAT_FSM_TYPE_UNSOLICITED);
                break;
        case CAT_UNSOLICITED_STATE_FORMAT_TEST_ARGS:
                s = format_test_args(self, CAT_FSM_TYPE_UNSOLICITED);
                break;
        case CAT_UNSOLICITED_STATE_READ_LOOP:
                s = process_read_loop(self, CAT_FSM_TYPE_UNSOLICITED);
                break;
        case CAT_UNSOLICITED_STATE_TEST_LOOP:
                s = process_test_loop(self, CAT_FSM_TYPE_UNSOLICITED);
                break;
        case CAT_UNSOLICITED_STATE_FLUSH_IO_WRITE_WAIT:
                s = unsolicited_process_io_write_wait(self);
                break;
        case CAT_UNSOLICITED_STATE_FLUSH_IO_WRITE:
                s = unsolicited_process_io_write(self);
                break;
        case CAT_UNSOLICITED_STATE_AFTER_FLUSH_RESET:
                unsolicited_reset_state(self);
                s = CAT_STATUS_BUSY;
                break;
        case CAT_UNSOLICITED_STATE_AFTER_FLUSH_OK:
                end_processing_with_ok(self, CAT_FSM_TYPE_UNSOLICITED);
                s = CAT_STATUS_BUSY;
                break;
        case CAT_UNSOLICITED_STATE_AFTER_FLUSH_FORMAT_READ_ARGS:
                start_processing_format_read_args(self, CAT_FSM_TYPE_UNSOLICITED);
                s = CAT_STATUS_BUSY;
                break;
        case CAT_UNSOLICITED_STATE_AFTER_FLUSH_FORMAT_TEST_ARGS:
                start_processing_format_test_args(self, CAT_FSM_TYPE_UNSOLICITED);
                s = CAT_STATUS_BUSY;
                break;
        default:
                break;
        }

        return s;
}

static bool is_unsolicited_fsm_busy(struct cat_object *self)
{
        return (self->unsolicited_fsm.state != CAT_UNSOLICITED_STATE_IDLE);
}

cat_status cat_service(struct cat_object *self)
{
        cat_status s;
        cat_status unsolicited_stat;

        assert(self != NULL);

        if ((self->mutex != NULL) && (self->mutex->lock() != 0))
                return CAT_STATUS_ERROR_MUTEX_LOCK;

        unsolicited_stat = unsolicited_events_service(self);

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
                s = format_read_args(self, CAT_FSM_TYPE_ATCMD);
                break;
        case CAT_STATE_WAIT_TEST_ACKNOWLEDGE:
                s = wait_test_acknowledge(self);
                break;
        case CAT_STATE_FORMAT_TEST_ARGS:
                s = format_test_args(self, CAT_FSM_TYPE_ATCMD);
                break;
        case CAT_STATE_WRITE_LOOP:
                s = process_write_loop(self);
                break;
        case CAT_STATE_READ_LOOP:
                s = process_read_loop(self, CAT_FSM_TYPE_ATCMD);
                break;
        case CAT_STATE_TEST_LOOP:
                s = process_test_loop(self, CAT_FSM_TYPE_ATCMD);
                break;
        case CAT_STATE_RUN_LOOP:
                s = process_run_loop(self);
                break;
        case CAT_STATE_HOLD:
                s = process_hold_state(self);
                break;
        case CAT_STATE_FLUSH_IO_WRITE_WAIT:
                s = process_io_write_wait(self);
                break;
        case CAT_STATE_FLUSH_IO_WRITE:
                s = process_io_write(self);
                break;
        case CAT_STATE_AFTER_FLUSH_RESET:
                reset_state(self);
                s = CAT_STATUS_BUSY;
                break;
        case CAT_STATE_AFTER_FLUSH_OK:
                ack_ok(self);
                s = CAT_STATUS_BUSY;
                break;
        case CAT_STATE_AFTER_FLUSH_FORMAT_READ_ARGS:
                start_processing_format_read_args(self, CAT_FSM_TYPE_ATCMD);
                s = CAT_STATUS_BUSY;
                break;
        case CAT_STATE_AFTER_FLUSH_FORMAT_TEST_ARGS:
                start_processing_format_test_args(self, CAT_FSM_TYPE_ATCMD);
                s = CAT_STATUS_BUSY;
                break;
        case CAT_STATE_PRINT_CMD:
                print_cmd_list(self);
                s = CAT_STATUS_BUSY;
                break;
        default:
                s = CAT_STATUS_ERROR_UNKNOWN_STATE;
                break;
        }

        if ((unsolicited_stat != CAT_STATUS_OK) || (is_unsolicited_fsm_busy(self) != false)) {
                s = CAT_STATUS_BUSY;
        }

        if ((self->mutex != NULL) && (self->mutex->unlock() != 0))
                return CAT_STATUS_ERROR_MUTEX_UNLOCK;

        return s;
}
