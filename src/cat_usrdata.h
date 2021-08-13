/****************************************************************************
 * src/cat_usrdata.h
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
#ifndef __CAT_USRDATA_H
#define __CAT_USRDATA_H

/*user define*/
#define WRITEBUF_LEN        256
#define READBUF_LEN         WRITEBUF_LEN
#define ACKBUF_LEN          512

/*cat user date buffer for write/read*/
typedef enum {
        CAT_USER_DATABUF_OPS_WRITE = 0,
        CAT_USER_DATABUF_OPS_READ,
        CAT_USER_DATABUF_OPS_MAX
} cat_user_data_ops_e;

/*cat user date buffer for ack_ok/ack_error */
typedef enum {
        CAT_USER_DATABUF_ACK_OK = 0,
        CAT_USER_DATABUF_ACK_ERR,
        CAT_USER_DATABUF_ACK_MAX
} cat_user_data_ack_e;

/*usr data buf*/
typedef struct cat_user_databuf_s
{
    uint8_t buf[WRITEBUF_LEN];
}cat_user_databuf_t;

/*usr data struct*/
typedef struct cat_user_data_s
{
    cat_user_databuf_t data_buf[CAT_USER_DATABUF_OPS_MAX][CAT_USER_DATABUF_ACK_MAX];
    uint16_t error_code;
}cat_user_data_t;


#define IS_DATA_OPS(n)  ((CAT_USER_DATABUF_OPS_WRITE == n) || \
                                (CAT_USER_DATABUF_OPS_READ == n))

#define IS_DATA_ACK(n)  ((CAT_USER_DATABUF_ACK_OK == n) || \
                                (CAT_USER_DATABUF_ACK_ERR == n))

bool cat_user_databuf_clear(cat_user_data_ops_e ops,cat_user_data_ack_e ack);
uint8_t *get_cat_user_databuf(cat_user_data_ops_e ops,cat_user_data_ack_e ack);
uint16_t get_cat_user_databuf_size(cat_user_data_ops_e ops,cat_user_data_ack_e ack);
uint16_t get_cat_user_databuf_errorcode(void);
void set_cat_user_databuf_errorcode(uint16_t val);
void cat_user_data_init(void);

#endif /*__CAT_USRDATA_H*/
