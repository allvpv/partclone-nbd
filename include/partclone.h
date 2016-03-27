/* Copyright © 2015-2016 Przemysław Kusiak
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef PARTCLONE_H_INCLUDED
#define PARTCLONE_H_INCLUDED

#include <inttypes.h>
#include <unistd.h>
#include <assert.h>

typedef enum {fatal = -2, error, ok} status;

/* short wrappers for GCC builtin functions */
#define swap16(x)       __builtin_bswap16(x)
#define swap32(x)       __builtin_bswap32(x)
#define swap64(x)       __builtin_bswap64(x)

/* why 8-bit integer? because on 64-bit machines it is the fastest algorithm */
#define popcount(x)     __builtin_popcountll(x)

/* shorter fixed types for storing integers */
typedef uint64_t    u64;
typedef int64_t     s64;
typedef uint32_t    u32;
typedef int32_t     s32;
typedef uint16_t    u16;
typedef int16_t     s16;
typedef uint8_t     u8;
typedef int8_t      s8;

/* fixed types for storing characters */
typedef int8_t      utf8;
typedef int16_t     utf16;
typedef int32_t     utf32;

/* format strings for printf */
#define fu8     "%"     PRIu8
#define fs8     "%"     PRId8
#define fu16    "%"     PRIu16
#define fs16    "%"     PRId16
#define fu32    "%"     PRIu32
#define fs32    "%"     PRId32
#define fu64    "%"     PRIu64
#define fs64    "%"     PRId64
#define fsize   "%zu"
#define fssize  "%zd"

/* useful macros */
#define kilobyte (1024)
#define megabyte (1024 * 1024)
#define gigabyte (1024 * 1024 * 1024)

#define divide_up(x, y) ((x) + ((y) - 1)) / (y)
#define divide_round(x, y) (((x) + 1) / (y))

#endif
