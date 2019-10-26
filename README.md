# libcat (cAT)
Plain C library for parsing AT commands.

## Features
* blazing fast and robust implementation
* 100% static implementation (without any dynamic memory allocation)
* very small footprint (both RAM and ROM)
* support for read, write and run type commands
* commands shortcuts (auto select best command candidate)
* CRLF and LF compatible
* case-insensitive
* dedicated for embedded systems
* object-oriented architecture
* separated interface for low-level layer
* multiplatform and portable
* asynchronous api with event callbacks
* only two source files
* unit tests

## Build

Build and install:

```sh
cmake .
make
make check
sudo make install
```

## Usage

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
                .name = "RESTART",
                .run = restart_run /* run handler for ATRESTART command */
        }
};
```

Define AT command parser descriptor:

```c

static char buf[128]; /* working buffer, must be declared manually */

static struct cat_descriptor desc = {
        .cmd = cmds,
        .cmd_num = sizeof(cmds) / sizeof(cmds[0]),

        .buf = working_buf,
        .buf_size = sizeof(working_buf),
};
```

Define IO low-level layer interface:

```c
int write_char(char ch)
{
        putc(ch);
        return 1;
}

int read_char(char *ch)
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