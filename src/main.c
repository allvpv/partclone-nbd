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

#include "partclone.h"
#include "options.h"
#include "log.h"
#include "initialization.h"
#include "nbd.h"

#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>

// God's number: 543632

int main(int argc, char **argv)
{
    /* -------------------- PARSE OPTIONS -------------------- */
    struct options options = {
        .image_path = NULL,
        .log_file = "partclone-nbd.log",
        .elems_per_cache = 512,
        .port = 10809,
        .syslog = 0,
        .quiet = 0
    };

    static struct option longopts[] = {
        {"port",                required_argument,  NULL, 'p'},
        {"elems-per-cache",     required_argument,  NULL, 'x'},
        {"help",                no_argument,        NULL, 'h'},
        {"log-file",            required_argument,  NULL, 'L'},
        {"quiet",               no_argument,        NULL, 'q'},
        {"syslog",              no_argument,        NULL, 'S'},
        {"silent",              no_argument,        NULL, 's'},
        {"version",             no_argument,        NULL, 'V'},
        {0},
    };

    for(;;) {
        int idx = 0;
        int opt = getopt_long(argc, argv, "p:x:hL:qSsV", longopts, &idx);

        if(opt == -1) break;

        switch(opt) {
        case 'p':
            options.port = atoi(optarg);
            break;

        case 'x':
            options.elems_per_cache = atoll(optarg);
            break;

        case 'h':
            printf(
                "Usage: partclone-nbd [OPTION...] partclone_image\n"
                "Serve a partclone image as a block device.\n"
                "\n"
//              "modes:\n"
//              "  -d, --daemon-mode          Run in background.\n"
//              "  -s, --standard-mode        Standard mode, enabled by default\n"
//              "\n"
//              "daemon mode options (works only with '-d'):\n"
//              "  -p, --pid-file=FILE        Specify a filename to write our PID to (default\n"
//              "                             path: /var/run/partclone-nbd.pid).\n"
//              "\n"
                "log_options:\n"
                "  -l, --filelog              Use a log file instead of syslog (default).\n"
                "  -S, --syslog               Use syslog instead of a log file.\n"
                "  -L, --log-file=FILE        Specify an alternative path for a log file.\n"
                "                             Default: partclone-nbd.log.\n"
                "\n"
                "image options:\n"
                "  -x, --elems-per-cache=NUM  Specify a number of bitmap elements per one cache\n"
                "                             element (default: 512). Higher value means better\n"
                "                             performance, but more RAM is consumed. Details in\n"
                "                             manual.\n"
                "\n"
                "NBD options:\n"
                "  -p, --port=NUM             Specify a port (default: 10809).\n"
//              "  -M, --max-connections=NUM  Specify a maximum number of opened connections.\n"
                "\n"
                "other options:\n"
                "  -h, --help                 Give this help list.\n"
                "  -q, --quiet                Do not print debug messages.\n"
                "  -V, --version              Print program version.\n"
                "\n"
                "Detalied descriptions of all options are available in the manual.\n"
            );

            return (int) ok;

        case 'V':
            printf("partclone-nbd v0.0.1\n");
            return (int) ok;

        case 'L':
            options.log_file = optarg;
            break;

        case 'S':
            options.syslog = 1;
            break;

        case 'q':
            options.quiet = 1;
            break;

        case '?':
            fprintf(stderr, "Type 'partclone-nbd --help' or 'man partclone'.\n");
            return (int) error;
        }
    }

    if(optind >= argc) {
        fprintf(stderr, "%s: no image file specified.\n", argv[0]);
        return (int) error;
    } else {
        options.image_path = argv[optind];
    }

    struct image img;

    if(initialize_log(&options) == error) goto error_1;
    if(load_image(&img, &options) == error) goto error_2;
    if(start_server(&img, &options) == error) goto error_3;
    if(close_image(&img) == error) goto error_2;
    if(close_log() == error) goto error_1;

    log_debug("Closing program with status 0.");
    return (int) ok;

error_3:
    close_image(&img);

error_2:
    log_error("Errors occured - see log file for details.");
    close_log();

error_1:
    return (int) error;
}
