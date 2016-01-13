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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "log.h"
#include "partclone.h"

static FILE *log_fd;

static int quiet = 0;

status initialize_log(struct options *options)
{
    quiet = options->quiet;

	if((log_fd = fopen(options->log_file ,"w")) == NULL) {
		fprintf(stderr, "Failed to open log file %s: %s\n", options->log_file, strerror(errno));
		return error;
	} else {
	    log_debug("Log initialized.");
	}

	return ok;
}

status close_log(void)
{
    if(fclose(log_fd) == EOF) {
        fprintf(stderr, "Cannot close log file: %s\n", strerror(errno));
        return error;
    }

    return ok;
}

NOT_LITERAL void log_msg(enum log_priority priority, const char *format, ...)
{
    va_list args;

    #define red     "\x1b[31m"
    #define green   "\x1b[32m"
    #define yellow  "\x1b[33m"
    #define blue    "\x1b[34m"
    #define magneta "\x1b[35m"
    #define cyan    "\x1b[36m"
    #define white   "\x1b[37m"

    #define bold    "\x1b[1m"
    #define blink   "\x1b[4m"
    #define reset   "\x1b[0m"

    const char* file_priority_string[4] = {
        "[ DBG ] ", "[ INF ] ", "[ WRN ] ", "[ ERR ] "
    };

    const char* print_priority_string[4] = {
        bold cyan    "[ DBG ] " reset,
        bold white   "[ INF ] " reset,
        bold yellow  "[ WRN ] " reset,
        bold red     "[ ERR ] " reset,
    };

    // ---------------------------------------------------------------------

    va_start(args, format);

    fprintf(log_fd, "%s", file_priority_string[priority]);
    vfprintf(log_fd, format, args);
    fprintf(log_fd, "\n");

    fflush(log_fd);

    if(quiet == 1 && priority == log_debug) return;

    // ----------------------------------------------------------------------

    FILE *stream;

    switch(priority)
    {
    /* to stderr */
    case log_error:
        stream = stderr;

    /* to stdout */
    case log_info:
    case log_warning:
    case log_debug:
        stream = stdout;

    default: // impossible case, but /forewarned is forearmed/
        stream = stdout;
    }

    va_end(args);
    va_start(args, format);

    fprintf(stream, "%s", print_priority_string[priority]);
    vfprintf(stream, format, args);
    fprintf(stream, "\n");
}
