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
#include "io.h"
#include "partclone.h"
#include "read_image.h"
#include "initialization.h"
#include "nbd.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

int get(int sock, void *buff, int count)
{
    int cnt = count;
    int pnt = 0;

    while(cnt > 0)
    {
        int once = recv(sock, buff, cnt, 0);

        if(once == 0) break;
        else if(once == -1) {
            log_error("Connection dropped: %s.", strerror(errno));
            break;
        }

        pnt += once;
        cnt -= once;
        buff += once;
    }

    return pnt;
}

ssize_t put(int sock, void *buff, size_t count)
{
    size_t cnt = count;
    size_t pnt = 0;

    while(cnt > 0) {
        ssize_t once = send(sock, buff, cnt, 0);

        if(once == 0) break;
        else if(once == -1) {
            log_error("Connection dropped: %s.", strerror(errno));
            break;
        }

        pnt += once;
        cnt -= once;
        buff += once;
    }

    if(pnt != count) log_error("Failed to send data to an image.");
    return pnt;
}

static inline status get16(int sock, u16 *v) {
    if(get(sock, v, 2) != 2) return error;
    *v = swap16(*v);
    return ok;
}

static inline status put16(int sock, u16 v) {
    v = swap16(v);
    if(put(sock, &v, 2) != 2) return error;
    return ok;
}

static inline status get32(int sock, u32 *v) {
    if(get(sock, v, 4) != 4) return error;
    *v = swap32(*v);
    return ok;
}

static inline status put32(int sock, u32 v) {
    v = swap32(v);
    if(put(sock, &v, 4) != 4) return error;
    return ok;
}

static inline status get64(int sock, u64 *v) {
    if(get(sock, v, 8) != 8) return error;
    *v = swap64(*v);
    return ok;
}

static inline status put64(int sock, u64 v) {
    v = swap64(v);
    if(put(sock, &v, 8) != 8) return error;
    return ok;
}

static status send_reply(int sock, u64 handle, u32 error_number)
{
    if(     put32(sock, 0x67446698) == error      ||
            put32(sock, error_number) == error    ||
            put64(sock, handle) == error          ){
    /* ---------------------------------------------------------------------- */
        log_error("Failed to send reply for the request.");
        return error;
    } else {
//        log_debug("Reply for the request sent succesfull.");

    }

    return ok;
}

struct args {
    int cl_sock;
    struct image *img;
};

void *server_thread(void *args)
{
    struct args *arguments = args;
    int sock = arguments->cl_sock;
    struct image *img = arguments->img;
    free(args); /* free memory allocated by argument */

    struct instance obj;

    if(create_instance(&obj, img) == error) {
        log_error("Cannot create an instance of the image.");
        goto error_3;
    } else {
        log_debug("Image instance created.");
    }

    void *zero =
        calloc(obj.img->block_size > 124 ? obj.img->block_size : 124, 1);

    void *buff =
        malloc(obj.img->block_size);

    void *read_buff;

    if(buff == NULL || zero == NULL) {
        log_error("Failed to allocate memory.");
        goto error_2;
    } else {
        log_debug("Memory allocated.");
    }

    // ====================================================================== //
    // ============================ NEGOTIATION ============================= //
    // ====================================================================== //

    char *magic1 = "NBDMAGIC";
    u64   magic2 = 0x49484156454F5054;

    // bit 0 - should be set by servers that support the fixed newstyle protocol
    // bit 1 - if set, and if the client replies with NBD_FLAG_C_NO_zero in
    //         the client flags field, the server MUST NOT send the 124 bytes of
    //         zero at the end of the negotiation.
    u16 flags1 = 0x00000000;

    // bit 0 (HAS_FLAGS) should always be 1
    // bit 1 should be set to 1 if the export is read-only
    // bit 2 should be set to 1 if the server supports NBD_CMD_FLUSH commands
    // bit 3 should be set to 1 if the server supports the NBD_CMD_FLAG_FUA flag
    // bit 4 should be set to 1 to let the client schedule I/O accesses as for a
    //       rotational medium
    // bit 5 NBD_FLAG_SEND_TRIM; should be set to 1 if the server supports
    //       NBD_CMD_TRIM commands
    u16 flags2 = 0x00000003; // bit 0 and 1 set

    u32 cl_flags;

    // send magic numbers
    if(put(sock, magic1, 8) != 8 ||
        put64(sock, magic2) == error) {
    // ---------------------------------------------------------------------- //
        log_error("Failed to send magic numbers.");
        goto error_1;
    } else {
        log_debug("Magic numbers sent.");
    }

    if(put16(sock, flags1) == error) {
        log_error("Failed to send global flags.");
        goto error_1;
    } else {
        log_debug("Global flags sent.");
    }

    if(get32(sock, &cl_flags)  == error) {
        log_error("Failed to receive global client flags.");
        goto error_1;
    } else {
        log_debug("Global client flags received.");
    }

    // bit 0 SHOULD be set by clients that support the fixed newstyle protocol.
    //       Servers MAY choose to honour fixed newstyle from clients that
    //       didn't set this bit, but relying on this isn't recommended.
    // bit 1 MUST NOT be set if the server did not set NBD_FLAG_NO_ZEROES. If
    //       set, the server MUST NOT send the 124 bytes of zero at the end of
    //       the negotiation.

    if(CHECK_BIT(cl_flags, 1)) {
        log_error("Flag NBD_FLAG_C_NOT_zero must not be set.");
        goto error_1;
    } else {
        log_debug("Flags parsed successfull.");
    }

    /* ---------------------------------------------------------------------- */

    u64 cl_magic;
    u32 cl_option, cl_length;

    if(get64(sock, &cl_magic)  == error ||
        get32(sock, &cl_option) == error ||
        get32(sock, &cl_length) == error ){
    /* ---------------------------------------------------------------------- */
        log_error("Failed to receive an option");
        goto error_1;
    } else {
        log_debug("Option received.");
    }

    if(cl_magic != magic2) {
        log_error("Unrecognized magic number received.");
        goto error_1;
    }

    // NBD_OPT_EXPORT_NAME (1) - This is the first and only option in nonfixed
    // newstyle handshade (which is used there).

    if(cl_option != 1) {
        log_error("Unrecognized option.");
        goto error_1;
    }

    // If cl_length != 0 the client tries to open custom export. It is not
    // allowed in our implementation.

    if(cl_length != 0) {
        log_error("Custom exports are not supported.");
        goto error_1;
    }

    log_debug("Option parsed. Sending reply...");


    if(put64(sock, obj.img->device_size) == error ||
        put16(sock, flags2)               == error ){
    /* ---------------------------------------------------------------------- */
        log_error("Failed to send reply for an option");
        goto error_1;
    } else {
        log_debug("Reply sent.");
    }

    if(put(sock, zero, 124) != 124) {
        log_msg(log_error,
                "Failed to send 124 bytes of zero ending negotiation.");
    } else {
        log_msg(log_debug,
                "124 bytes of zero ending negotiation sent successufull.");
    }

    // ====================================================================== //
    // ======================== REQUEST & REPLIES =========================== //
    // ====================================================================== //

    /* main loop */
    for(;;)
    {
        u32 magic, count, type;
        u64 seek, handle;

        if(get32(sock, &magic) == error    || // 0x25609513
            get32(sock, &type) == error    || // 0 -read
            get64(sock, &handle) == error  || // handle
            get64(sock, &seek) == error    || // seek
            get32(sock, &count) == error   ){ // length
        // ----------------------------------------------------------------- //
            log_error("Failed to read request.");
            break;
        }

        if(magic != 0x25609513) {
            log_error("Parsing request: Bad magic.");
        }

        // verify request
        if(seek > obj.img->device_size || count +seek > obj.img->device_size) {
        // ----------------------------------------------------------------- //
            log_msg(log_error,
                    "Parsing request: Offset is beyond the end of the image.");
            if(send_reply(sock, handle, EINVAL) == ok) continue;
            else break;
        // write (1), flush (3) or trim (4) on a RO device is not permited
        } else if(type == 1 || type == 3 || type == 4) {
            log_error("Parsing request: Unexpected operation in "
                               "RO mode.");
            if(send_reply(sock, handle, EPERM) == ok) continue;
            else break;
        } else if(type == 2) { /* disconnect request */
            log_error("Client sent a disconnect request.");
            break;
        } else if(type != 0) { /* unknown request; 0 - read request */
            log_error("Parsing request: Unexpected request type.");
            break;
        }

        if(send_reply(sock, handle, 0) == error) break;

        u64 block  = seek / obj.img->block_size;
        u32 offset = seek % obj.img->block_size;

        if(set_block(&obj, block) == error) goto error_1;
        if(offset_in_current_block(&obj, offset) == error) goto error_1;

        for (;count > 0;) {

            u32 once_read = MIN(obj.o.remaining_bytes, count);
            count -= once_read;
            obj.o.remaining_bytes -= once_read;

            // ------------------------------------------------------------- //

            if(obj.o.existence == 0) {
                read_buff = zero;

            } else if(read_whole(obj.o.fd, buff, once_read) != error) {
                read_buff = buff;

            } else {
                log_error("Failed to read some data from the image.");
                goto error_1;
            }

            // ------------------------------------------------------------- //

            if(put(sock, read_buff, once_read) != once_read) {
                log_error("Failed to send data to the image.");
                goto error_1;
            }

            next_block(&obj);
        }
    }

error_1:
    free(buff);

error_2:
    if(close_instance(&obj) == error) {
        log_error("Failed to close an image instance.");
    } else {
        log_debug("Image instance closed.");
    }

error_3:
    if(close(sock) == -1) {
        log_error("Failed to close client sock: %s.", strerror(errno));
    } else {
        log_debug("Client sock closed.");
    }

    log_debug("Thread closed.");
    log_info("Waiting for new connections ...");

    pthread_exit(0);
    return 0;
}

status start_server(struct image *img, struct options *options)
{
    // create listener socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if(sock == -1) {
        log_error("Failed to create a socket: %s.", strerror(errno));
        return error;
    } else {
        log_debug("Socket created.");
    }

    // allow to reuse address
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1) {
        log_error("Failed to reuse address: %s.", strerror(errno));
    } else {
        log_debug("Address reused.");
    }

    // bind port to a socket
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof server_addr);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(options->port);

    if(bind(sock, (struct sockaddr*) &server_addr, sizeof server_addr) == -1) {
        log_error("Failed to bind port to a socket: %s.", strerror(errno));
        goto error;
    } else {
        log_debug("Socket binded to a port.");
    }

    // start listening on a port
    if(listen(sock, 5) == -1) {
        log_error("Failed to start listening on a port.");
        goto error;
    } else {
        log_debug("Listening on a port started.");
    }

    log_info("Server initialized. Listening on a port %i", options->port);

    for(;;) {
        /* allocate memory for an argument */
        struct args *arg = malloc(sizeof *arg);

        if(arg == NULL) {
            log_error("Cannot allocate memory for thread arguments.");
            continue;
        }

        /* accept a connection */
        struct sockaddr_in clientaddr;
        socklen_t clientaddrlen = sizeof clientaddr;

        int cl_sock = accept(sock, (struct sockaddr*) &clientaddr, &clientaddrlen);

        if(cl_sock == -1) {
            log_error("Failed to accept a connection.");
            free(arg);
            continue;
        }

        arg->cl_sock = cl_sock;
        arg->img = img;

        log_info("Connection made with %s.", inet_ntoa(clientaddr.sin_addr));


        pthread_t t;

        if(pthread_create(&t, NULL, server_thread, arg) == error) {
            log_error("Failed to create server thread: %s.", strerror(errno));
            free(arg);
            goto error;
        } else {
            log_debug("Thread created.");
        }
    }

    return ok;

error:
    if(close(sock) == -1) {
        log_error("Failed to close main sock: %s.", strerror(errno));
    } else {
        log_debug("Main sock closed.");
    }

    return error;
}
