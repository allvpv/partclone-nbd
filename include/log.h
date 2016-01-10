/*
 * Copyright (c) 2015 Przemysław Kusiak <primo@poczta.fm>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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
