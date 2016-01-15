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

#include "log.h"
#include "partclone.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

static FILE *log_fd;

static int quiet;
static int syslog_enabled;

status initialize_log(struct options *options)
{
    quiet = options->quiet;
    syslog_enabled = options->syslog;

    if(syslog_enabled) {

        if(fclose(stdout) == EOF || fclose(stderr) == EOF) {
            fprintf(stderr, "Cannot close stdout and stderr: %s", strerror(errno));
            return error;
        }

        openlog("partclone-nbd", LOG_NDELAY, LOG_USER);

        log_msg(log_debug, "System logger initialized.");


    } else {
        if((log_fd = fopen(options->log_file ,"w")) == NULL) {
            fprintf(stderr, "Failed to open log file %s: %s\n", options->log_file, strerror(errno));
            return error;
        } else {
            log_debug("Log file initialized.");
        }
    }



    return ok;
}

status close_log(void)
{
    if(syslog_enabled) {

        closelog();

    } else {

        if (fclose(log_fd) == EOF) {
            fprintf(stderr, "Cannot close log file: %s\n", strerror(errno));
            return error;
        }

    }

    return ok;
}

NOT_LITERAL void log_msg(enum log_priority priority, const char *format, ...)
{
    va_list args;


    if(syslog_enabled) {

        int syslog_priority_mapper[4] = {
            LOG_DEBUG,
            LOG_INFO,
            LOG_WARNING,
            LOG_ERR
        };

        if(quiet && !priority) return;

        va_start(args, format);

        vsyslog(syslog_priority_mapper[priority], format, args);

        va_end(args);

    } else {

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

        va_end(args);

        // ----------------------------------------------------------------------

        FILE *stream;

        switch(priority)
        {
        /* to stderr */
        case log_error:
            stream = stderr;
            break;

        case log_debug:
            if(quiet) return;
            // no break!

        // to stdout
        case log_info:
        case log_warning:
            stream = stdout;
        }

        va_start(args, format);

        fprintf(stream, "%s", print_priority_string[priority]);
        vfprintf(stream, format, args);
        fprintf(stream, "\n");

        va_end(args);
    }
}
