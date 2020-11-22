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
#include "image.h"
#include "nbd.h"

#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>

// God's number: 543632

int main(int argc, char **argv)
{
    /* -------------------- PARSE OPTIONS -------------------- */

    // assing default values
    struct options options = {
        .device_path = "/dev/nbd0",
        .image_path = NULL,
        .custom_log_file = 0,
        .log_file = "/var/log/partclone-nbd.log",
        .elems_per_cache = 512,
        .server_mode = 0,
        .client_mode = 0,
        .port = 10809,
        .debug = 0,
        .quiet = 0
    };

    static struct option longopts[] = {
        {"port",                required_argument,  NULL, 'p'},
        {"elems-per-cache",     required_argument,  NULL, 'x'},
        {"help",                no_argument,        NULL, 'h'},
        {"quiet",               no_argument,        NULL, 'q'},
        {"server-mode",         no_argument,        NULL, 's'},
        {"client-mode",         no_argument,        NULL, 'c'},
        {"device",              required_argument,  NULL, 'd'},
        {"log-file",            required_argument,  NULL, 'L'},
        {"debug",               no_argument,        NULL, 'D'},
        {"version",             no_argument,        NULL, 'V'},
        {0},
    };

    for(;;) {
        int idx = 0; // it'll be incremented
        int opt = getopt_long(argc, argv, "p:d:x:hL:DqscV", longopts, &idx);

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
                "modes:\n"
                "  -c, --client-mode          Create a block device locally\n"
                "  -s, --server-mode          Listen on a port for clients.\n"
                "\n"
                "log_options:\n"
                "  -L, --log-file=FILE        Specify an alternative path for a log file.\n"
                "                             Default: partclone-nbd.log.\n"
                "  -D, --debug                Print debug messages to stdout.\n"
                "\n"
                "image options:\n"
                "  -x, --elems-per-cache=NUM  Specify a number of bitmap elements per one cache\n"
                "                             element (default: 512). Higher value means better\n"
                "                             performance, but more RAM is consumed. Details in\n"
                "                             manual.\n"
                "\n"
                "server mode options:\n"
                "  -p, --port=NUM             Specify a port (default: 10809).\n"
                "\n"
                "client mode options:\n"
                "  -d, --device=DEV           Specify another NBD device (default: /dev/nbd0)\n"
                "\n"
                "other options:\n"
                "  -h, --help                 Give this help list.\n"
                "  -q, --quiet                Print messages only to log file.\n"
                "  -V, --version              Print program version.\n"
                "\n"
            );

            return (int) ok;

        case 'V':
            printf("partclone-nbd v0.0.3\n");
            return (int) ok;

        case 'L':
            options.log_file = optarg;
            break;

        case 'D':
            options.debug = 1;
            break;

        case 'c':
            if(options.server_mode) {
                fprintf(stderr, "You can specify only one mode!\n");
                return (int) error;
            }

            options.client_mode = 1;
            break;

        case 's':
            if(options.client_mode) {
                fprintf(stderr, "You can specify only one (client or server) mode!\n");
                return (int) error;
            }

            options.server_mode = 1;

            break;

        case 'd':
            options.device_path = optarg;
            break;

        case 'q':
            options.quiet = 1;
            break;

        case '?':
            fprintf(stderr, "Type 'partclone-nbd --help'.\n");
            return (int) error;
        }
    }

    // check if image is given
    if(optind >= argc) {
        fprintf(stderr, "%s: no image file specified.\n", argv[0]);
        return (int) error;
    } else {
        options.image_path = argv[optind];
    }

    // check if mode is set
    if(!options.client_mode) if(!options.server_mode) {
        fprintf(stderr, "%s: you must specify a mode.\n", argv[0]);
        return (int) error;
    }

    struct image img;

    if(initialize_log(&options) == error) goto error_1;
    if(load_image(&img, &options) == error) goto error_2;
    // it is a mess with client and server mode (methods); see nbd.c, everything
    // is explained in comments (somwhere in the middle of the file).
    if(options.client_mode) if(start_client(&img, &options) == error) goto error_3;
    if(options.server_mode) if(start_server(&img, &options) == error) goto error_3;
    if(close_image(&img) == error) goto error_2;
    if(close_log() == error) goto error_1;

    log_debug("Closing program with status 0.");
    return (int) ok;

error_3:
    close_image(&img);

error_2:
    log_error("Errors occured - see log file \"%s\" for details.", options.log_file);
    close_log();

error_1:
    return (int) error;
}
