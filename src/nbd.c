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
#include "image.h"
#include "nbd.h"
#include "signals.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stropts.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/sendfile.h>

// Macros from Linux kernel headers nbd.h and fs.h. Needed in start_client() method.

#define NBD_SET_SOCK        _IO( 0xab, 0 )
#define NBD_SET_BLKSIZE     _IO( 0xab, 1 )
#define NBD_SET_SIZE        _IO( 0xab, 2 )
#define NBD_DO_IT           _IO( 0xab, 3 )
#define NBD_CLEAR_SOCK      _IO( 0xab, 4 )
#define NBD_CLEAR_QUE       _IO( 0xab, 5 )
#define NBD_PRINT_DEBUG     _IO( 0xab, 6 )
#define NBD_SET_SIZE_BLOCKS _IO( 0xab, 7 )
#define NBD_DISCONNECT      _IO( 0xab, 8 )
#define NBD_SET_TIMEOUT     _IO( 0xab, 9 )
#define NBD_SET_FLAGS       _IO( 0xab, 10)
#define BLKROSET            _IO( 0x12, 93) /* set RO */


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
    }

    return ok;
}

static status WORKER(int sock, struct image *img)
{
    void *zero;

    // calloc do malloc and fills buffer with zeroes
    zero = calloc(img->block_size, 1); // "1" means size, NOT "fill with 1"

    if(zero == NULL) /* allocation failed */ {
        log_error("Cannot allocate memory for storing a chunk.");
        goto error_1;
    } else {
        log_debug("Memory for storing a chunk allocated.");
    }
    
    // set signal return point; must be before initialize_handling() (!)
    int sig_num = sigsetjmp(env, 1); 

    if (sig_num != 0) {
        cleanup_after_signal_handling();
        log_error("Signal \"%s\" caught.", strsignal(sig_num));
        goto error_3;
    } else {
        log_debug("Signal return point set.");
    }
    
    initialize_handling(); // initialize signal handling

    // ====================================================================== //
    // ======================== REQUEST & REPLIES =========================== //
    // ====================================================================== //
    
    log_info("Waiting for requests ...");

    /* the great loop */
    for(;;)
    {
        u32 magic, count, type;
        u64 seek, handle;

        if(get32(sock, &magic) == error   || // 0x25609513
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
        if(seek > img->device_size || count + seek > img->device_size) {
        // ----------------------------------------------------------------- //
            log_msg(log_error,
                    "Parsing request: Offset is beyond the end of the image.");
            if(send_reply(sock, handle, EINVAL) == ok) continue;
            else break;
        // write (1), flush (3) or trim (4) on a RO device is not permitted
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

        u64 block  = seek / img->block_size;
        u32 offset = seek % img->block_size;

        if(set_block(img, block) == error) goto error_3;
        if(offset_in_current_block(img, offset) == error) goto error_3;

        // send all chunks; size of chunk = size of image block (see *buff)
        for (;count > 0;) {

            u32 once_read = MIN(img->o_remaining_bytes, count);
            count -= once_read;
            img->o_remaining_bytes -= once_read;

            // ------------------------------------------------------------- //

            if(img->o_existence == 0) {

                if(put(sock, zero, once_read) != once_read) {
                    log_error("Failed to write some zeroes to device: %s.", strerror(errno));
                    goto error_3;
                }

            } else {
                
                // direct transmission from device to device using sendfile
                if(sendfile(sock, img->fd, NULL, once_read) != once_read) {
                    log_error("Failed to send some data from image to device: %s.", strerror(errno));
                    goto error_3;
                }

            }

            // ------------------------------------------------------------- //

            if (next_block(img) == error) goto error_3;
        }
    }

error_3:
    free(zero);

error_1:
    log_error("WORKER closed ...");
    return error;
}

// SERVER MODE vs CLIENT MODE

// Both client mode and server mode try to gain socket to communicate with a
// client. Client can be on the same computer as partclone-nbd, or somewhere in
// the internet.

// Server mode can serve image to another computers through the INET socks.

// Client mode is much faster than server, because it uses UNIX(r) sockets,
// not INET sockets. But, of course, it's limited to one machine.

// Both client mode and server mode finally call WORKER function.

/* ------------------------- SERVER MODE --------------------------------- */

static status server_negotiation(int sock, struct image *img);

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
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons(options->port)
    };

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

    log_info("Server initialized. Listening on a port %i ...", options->port);

    // the server loop
    for(;;) {

        /* accept a connection */
        struct sockaddr_in clientaddr;
        socklen_t clientaddrlen = sizeof clientaddr;

        int cl_sock = accept(sock, (struct sockaddr*) &clientaddr, &clientaddrlen);

        if(cl_sock == -1) {
            log_error("Failed to accept a connection.");
            continue;
        }

        log_info("Connection made with %s.", inet_ntoa(clientaddr.sin_addr));
        server_negotiation(cl_sock, img);
    }

error:
    if(close(sock) == -1) {
        log_error("Failed to close main sock: %s.", strerror(errno));
    } else {
        log_debug("Main sock closed.");
    }

    return error;
}

static status server_negotiation(int sock, struct image *img)
{
    u8 zero[124] = { 0 };

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
    // newstyle handshake (which is used there).

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


    if(put64(sock, img->device_size) == error ||
       put16(sock, flags2)               == error ){
    /* ---------------------------------------------------------------------- */
        log_error("Failed to send reply for an option");
        goto error_1;
    } else {
        log_debug("Reply sent.");
    }

    if(put(sock, &zero, 124) != 124) {
        log_error("Failed to send 124 bytes of zero ending negotiation.");
    } else {
        log_debug("124 bytes of zero ending negotiation sent successufull.");
    }

    // FINALLY we gained client socket

    WORKER(sock, img); 
    log_info("... so we are waiting for new connections.");

error_1:
    if(close(sock) == -1) {
        log_error("Failed to close client sock: %s.", strerror(errno));
    } else {
        log_debug("Client sock closed.");
    }

    log_debug("Thread closed.");
    log_info("Waiting for new connections ...");

    return error;
}

/* ------------------------- CLIENT MODE ---------------------------------- */

// this thread imitates a client.
void *lock_on_do_it(void *devsock_addr)
{
    int devsock = *(int*) devsock_addr;

    if (ioctl(devsock, NBD_DO_IT) == -1) {
        log_error("Failed to lock on NBD_DO_IT: %s.", strerror(errno));
    }

    log_debug("Locking finished.");

    pthread_exit(0);
}


status start_client(struct image *img, struct options *options)
{
    int socket[2]; // socket[0] goes to the kernel, socket[1] to us

    if (socketpair(PF_UNIX, SOCK_STREAM, 0, socket) == -1) {
        log_error("Cannot create a pair of sockets: %s.", strerror(errno));
        goto error_2;
    } else {
        log_debug("A pair of sockets created.");
    }

    int communication_sock = socket[1];
    int kernel_sock = socket[0];
    int device_sock = open(options->device_path, O_RDWR);

 
    if (device_sock == -1) {
        log_error("Failed to open %s device in RW mode: %s.", options->device_path, strerror(errno));
        goto error_3;
    } else {
        log_debug("%s device opened.", options->device_path);
    }

    if (ioctl(device_sock, NBD_CLEAR_SOCK) == -1) {
        log_error("Failed to clear a NBD device socket: %s.", strerror(errno));
        goto error_4;
    } else {
        log_debug("NBD device socket cleared.");
    }

    if (ioctl(device_sock, NBD_SET_SOCK, kernel_sock) == -1) {
        log_error("Failed to set socket for communication with kernel: %s.", strerror(errno));
        goto error_4;
    } else {
        log_debug("Socket for communication with kernel set.");
    }

    if (ioctl(device_sock, NBD_SET_BLKSIZE, img->block_size) == -1) {
        log_error("Failed to send image block size (" fu32 "): %s.", img->block_size, strerror(errno));
        goto error_5;
    } else {
        log_debug("Image block size (" fu32 ") sent.", img->block_size);
    }

    if (ioctl(device_sock, NBD_SET_SIZE_BLOCKS, img->blocks_count) == -1) {
        log_error("Failed to send number of blocks (" fu64 "): %s.", img->blocks_count, strerror(errno));
        goto error_5;
    } else {
        log_debug("Number of blocks (" fu64 ") sent.", img->blocks_count);
    }

    if (ioctl(device_sock, BLKROSET, &(int){1}) == -1) {
        log_error("Failed to set read only device attribute: %s.", strerror(errno));
        goto error_5;
    } else {
        log_debug("Read only device attribute set.");
    }

    pthread_t thread;

    // very hackish and complicated; lock thread imitate a client; this thread
    // imitate a server. Quoting official nbd client:

            /* Due to a race, the kernel NBD driver cannot
             * call for a reread of the partition table
             * in the handling of the NBD_DO_IT ioctl().
             * Therefore, this is done in the first open()
             * of the device. We therefore make sure that
             * the device is opened at least once after the
             * connection was made. This has to be done in a
             * separate process, since the NBD_DO_IT ioctl()
             * does not return until the NBD device has
             * disconnected. */

    if(pthread_create(&thread, NULL, lock_on_do_it, &device_sock) == -1) {
        log_error("Failed to create lock thread.");
        goto error_5;
    } else {
        log_debug("Lock thread created.");
    }

    close(kernel_sock);

    WORKER(communication_sock, img);

    // if WORKER returned, it means error so ...

error_5:
    if (ioctl(device_sock, NBD_DISCONNECT) == -1) {
        log_error("Cannot disconnect from NBD device");
        goto error_4;
    } else {
        log_debug("Disconnected from NBD device.");
    }

    /*if (ioctl(device_sock, NBD_CLEAR_SOCK) == -1) {*/
        /*log_error("Failed to clear a NBD device socket: %s.", strerror(errno));*/
        /*goto error_4;*/
    /*} else {*/
        /*log_debug("NBD device socket cleared.");*/
    /*}*/

    pthread_join(thread, NULL);

error_4:
    close(device_sock);
    
error_3:
    close(kernel_sock);
    close(communication_sock);

error_2:
    log_msg(log_error, "Failed to initialize NBD device.");
    return error;
}
