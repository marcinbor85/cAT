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

#include <stdio.h>

/* include library header */
#include <ihex.h>

/* example intelhex data file content */
static char input_hex[] =
":020000040800F2\n"
":0400020001020304F0\n"
":00000001FF\n";

int main(int argc, char **argv)
{
        struct ihex_object *ihex;
        FILE *fp;
        unsigned char data[8];
        int i;

        /* create new intelhex parser instance */
	ihex = ihex_new();

        /* open file with intelhex data */
	fp = fmemopen(input_hex, sizeof(input_hex), "r");

        /* parsing intelhex stream */
        ihex_parse_file(ihex, fp);

        /* close file */
	fclose(fp);

        /* get binary data */
        ihex_get_data(ihex, 0x08000000, data, sizeof(data));

        /* print data output: FF FF 01 02 03 04 FF FF*/
        for (i = 0; i < sizeof(data); i++)
                printf("%02X ", data[i]);

        /* free resources */
        ihex_delete(ihex);

        return 0;
}