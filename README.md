[![Build Status](https://travis-ci.org/marcinbor85/cat.svg?branch=master)](https://travis-ci.org/marcinbor85/cat)
# libcat (cAT)
Plain C library for parsing AT commands for use in host devices.

## Features
* blazing fast, non-blocking, robust implementation
* 100% static implementation (without any dynamic memory allocation)
* very small footprint (both RAM and ROM)
* support for READ, WRITE, TEST and RUN type commands
* commands shortcuts (auto select best command candidate)
* single request - multiple responses
* unsolicited read/test command support
* hold state for delayed responses for time-consuming tasks
* high-level memory variables mapping arguments parsing
* variables accessors (read and write, read only, write only)
* automatic arguments types validating
* automatic format test responses for commands with variables
* CRLF and LF compatible
* case-insensitive
* dedicated for embedded systems
* object-oriented architecture
* separated interface for low-level layer
* fully asynchronous input/output operations
* multiplatform and portable
* asynchronous api with event callbacks
* print registered commands list feature
* only two source files
* wide unit tests

## Build

Build and install:

```sh
cmake .
make
make test
sudo make install
```

## Example basic demo posibilities

```console
AT+PRINT=?                                              # TEST command
+PRINT=<X:UINT8[RW]>,<Y:UINT8[RW]>,<MESSAGE:STRING[RW]> # Automatic response
Printing something special at (X,Y).                    # Automatic response
OK                                                      # Automatic acknowledge

AT+PRINT?                                               # READ command
+PRINT=0,0,""                                           # Automatic response
OK                                                      # Automatic acknowledge

AT+PRINT=xyz,-2                                         # WRITE command
ERROR                                                   # Automatic acknowledge

AT+PRINT=1,2,"test"                                     # WRITE command
OK                                                      # Automatic acknowledge

AT+PRINT                                                # RUN command
some printing at (1,2) with text "test"                 # Manual response
OK                                                      # Automatic acknowledge
```

## Example unsolicited demo posibilities

```console
AT+START=?                                              # TEST command
+START=<MODE:UINT32[WO]>                                # Automatic response
Start scanning after write (0 - wifi, 1 - bluetooth).   # Automatic response
OK                                                      # Automatic acknowledge

AT+START=0                                              # WRITE command
+SCAN=-10,"wifi1"                                       # Unsolicited read response
+SCAN=-50,"wifi2"                                       # Unsolicited read response
+SCAN=-20,"wifi3"                                       # Unsolicited read response
OK                                                      # Unsolicited acknowledge

AT+START=1                                              # WRITE command
+SCAN=-20,"bluetooth1"                                  # Unsolicited read response
OK                                                      # Unsolicited acknowledge

AT+SCAN=?                                               # TEST command
+SCAN=<RSSI:INT32[RO]>,<SSID:STRING[RO]>                # Automatic response
Scan result record.                                     # Automatic response
OK                                                      # Automatic acknowledge
```

## Usage

Define High-Level variables:

```c

static uint8_t x;
static uint8_t y;
static char msg[32];

static struct cat_variable go_vars[] = {
        {
                .type = CAT_VAR_UINT_DEC, /* unsigned int variable */
                .data = &x,
                .data_size = sizeof(x),
                .write = x_write,
                .name = "X",
                .access = CAT_VAR_ACCESS_READ_WRITE,
        },
        {
                .type = CAT_VAR_UINT_DEC, /* unsigned int variable */
                .data = &y,
                .data_size = sizeof(y),
                .write = y_write,
                .access = CAT_VAR_ACCESS_READ_WRITE,
        },
        {
                .type = CAT_VAR_BUF_STRING, /* string variable */
                .data = msg,
                .data_size = sizeof(msg),
                .write = msg_write,
                .access = CAT_VAR_ACCESS_READ_WRITE,
        }
};
```

Define AT commands descriptor:

```c
static struct cat_command cmds[] = {
        {
                .name = "TEST",
                .read = test_read, /* read handler for ATTEST? command */
                .write = test_write, /* write handler for ATTEST={val} command */
                .run = test_run /* run handler for ATTEST command */
        },
        {
                .name = "+NUM",
                .write = num_write, /* write handler for AT+NUM={val} command */
                .read = num_read /* read handler for AT+NUM? command */
        },
        {
                .name = "+GO",
                .write = go_write, /* write handler for AT+GO={x},{y},{msg} command */
                .var = go_vars, /* attach variables to command */
                .var_num = sizeof(go_vars) / sizeof(go_vars[0]),
                .need_all_vars = true
        },
        {
                .name = "RESTART",
                .run = restart_run /* run handler for ATRESTART command */
        }
};
```

Define AT command parser descriptor:

```c

static char working_buf[128]; /* working buffer, must be declared manually */

static struct cat_command_group cmd_group = {
        .cmd = cmds,
        .cmd_num = sizeof(cmds) / sizeof(cmds[0]),
};

static struct cat_command_group *cmd_desc[] = {
        &cmd_group
};

static struct cat_descriptor desc = {
        .cmd_group = cmd_desc,
        .cmd_group_num = sizeof(cmd_desc) / sizeof(cmd_desc[0]),

        .buf = working_buf,
        .buf_size = sizeof(working_buf),
};
```

Define IO low-level layer interface:

```c
static int write_char(char ch)
{
        putc(ch, stdout);
        return 1;
}

static int read_char(char *ch)
{
        *ch = getch();
        return 1;
}

static struct cat_io_interface iface = {
        .read = read_char,
        .write = write_char
};
```

Initialize AT command parser and run:

```c
struct cat_object at; /* at command parser object */

cat_init(&at, &desc, &iface, NULL); /* initialize at command parser object */

while (1) {
        cat_service(&at) /* periodically call at command parser service */

        ... /* other stuff, running in main loop */
}

```
