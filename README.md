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
* separated generic low-layer
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
