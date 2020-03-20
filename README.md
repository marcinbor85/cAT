[![Build Status](https://travis-ci.org/marcinbor85/cat.svg?branch=master)](https://travis-ci.org/marcinbor85/cat)
# libcat (cAT)
Plain C library for parsing AT commands.

## Features
* blazing fast and robust implementation
* 100% static implementation (without any dynamic memory allocation)
* very small footprint (both RAM and ROM)
* support for READ, WRITE, TEST and RUN type commands
* commands shortcuts (auto select best command candidate)
* high-level memory variables mapping arguments parsing
* automatic arguments types validating
* automatic format test responses for commands with variables
* CRLF and LF compatible
* case-insensitive
* dedicated for embedded systems
* object-oriented architecture
* separated interface for low-level layer
* multiplatform and portable
* asynchronous api (only 2 functions) with event callbacks
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

## Example demo posibilities

```console
at+print=?                                      # TEST command
+PRINT=<X:UINT8>,<Y:UINT8>,<MESSAGE:STRING>     # Automatic response
OK                                              # Automatic acknowledge

at+print?                                       # READ command
+PRINT=0,0,""                                   # Automatic response
OK                                              # Automatic acknowledge

at+print=xyz,-2                                 # WRITE command
ERROR                                           # Automatic acknowledge

at+print=1,2,"test"                             # WRITE command
OK                                              # Automatic acknowledge

at+print                                        # RUN command
some printing at (1,2) with text "test"         # Manual response
OK                                              # Automatic acknowledge
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
                .name = "X"
        },
        {
                .type = CAT_VAR_UINT_DEC, /* unsigned int variable */
                .data = &y,
                .data_size = sizeof(y),
                .write = y_write
        },
        {
                .type = CAT_VAR_BUF_STRING, /* string variable */
                .data = msg,
                .data_size = sizeof(msg),
                .write = msg_write
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

static struct cat_descriptor desc = {
        .cmd = cmds,
        .cmd_num = sizeof(cmds) / sizeof(cmds[0]),

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

cat_init(&at, &desc, &iface); /* initialize at command parser object */

while (1) {
        cat_service(&at) /* periodically call at command parser service */

        ... /* other stuff, running in main loop */
}

```