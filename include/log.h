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

#ifndef LOG_H_INCLUDED
#define LOG_H_INCLUDED

#include "options.h"
#include "partclone.h"

#include <stdio.h>

// GCC way to avoid "-Wformat-nonliteral" warnings
#define NOT_LITERAL __attribute__((format(printf, 2, 3)))

#define log_info(f_, ...) log_msg(log_info, (f_), ##__VA_ARGS__)
#define log_debug(f_, ...) log_msg(log_debug, (f_), ##__VA_ARGS__)
#define log_warning(f_, ...) log_msg(log_warning, (f_), ##__VA_ARGS__)
#define log_error(f_, ...) log_msg(log_error, (f_), ##__VA_ARGS__)

enum log_priority{log_debug, log_info, log_warning, log_error};

status initialize_log(struct options *options);
status close_log(void);
NOT_LITERAL void log_msg(enum log_priority priority, const char *format, ...);

#endif
