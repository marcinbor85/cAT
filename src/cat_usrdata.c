/****************************************************************************
 * src/cat_usrdata.c
 *
 * Copyright (C) 2021 Xiaomi Corperation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include "cat.h"
#include "cat_usrdata.h"

static cat_user_data_t cat_user_data;
/**
 * @description:clear databuf
 * @param {cat_user_data_ops_e} ops:read/write
 * @param {cat_user_data_ack_e} ack:ok/error
 * @return:success/fail
 */
bool cat_user_databuf_clear(cat_user_data_ops_e ops,cat_user_data_ack_e ack)
{
        assert(IS_DATA_OPS(ops));
        assert(IS_DATA_ACK(ack));
        memset((cat_user_data.data_buf[ops][ack].buf),0,WRITEBUF_LEN);
        return true;
}

/**
 * @description:get databuf
 * @param {cat_user_data_ops_e} ops:read/write
 * @param {cat_user_data_ack_e} ack:ok/error
 * @return:databuf pointer
 */
uint8_t *get_cat_user_databuf(cat_user_data_ops_e ops,cat_user_data_ack_e ack)
{
        assert(IS_DATA_OPS(ops));
        assert(IS_DATA_ACK(ack));
        return (uint8_t *)(cat_user_data.data_buf[ops][ack].buf);
}

/**
 * @description:get databuf size
 * @param {cat_user_data_ops_e} ops:read/write
 * @param {cat_user_data_ack_e} ack:ok/error
 * @return:databuf size
 */
uint16_t get_cat_user_databuf_size(cat_user_data_ops_e ops,cat_user_data_ack_e ack)
{
        assert(IS_DATA_OPS(ops));
        assert(IS_DATA_ACK(ack));
        return sizeof(cat_user_data.data_buf[ops][ack].buf);
}

/**
 * @description:get errorcode
 * @param {*}
 * @return:errorcode
 */
uint16_t get_cat_user_databuf_errorcode(void)
{
        return cat_user_data.error_code;
}

/**
 * @description:set errorcode
 * @param {uint16_t} val
 * @return {*}
 */
void set_cat_user_databuf_errorcode(uint16_t val)
{
        cat_user_data.error_code = val;
}

/**
 * @description:userdata initialization
 * @param {*}
 * @return {*}
 */
void cat_user_data_init(void)
{
        memset(&cat_user_data,0,sizeof(cat_user_data));
}
