# libcat (cAT)
Plain C library for parsing AT commands.

## Features
* blazing fast and robust implementation
* 100% static implementation
* very small footprint (both RAM and ROM)
* commands shortcuts (auto select best command candidate)
* CRLF and LF compatible
* dedicated for embedded systems
* asynchronous api with events callbacks
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
