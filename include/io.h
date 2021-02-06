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

#ifndef IO_H_INCLUDED
#define IO_H_INCLUDED

#include "partclone.h"
#include "log.h"

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static inline status read_whole(int fd, void *dest, size_t size)
{
    ssize_t once_read;

    while ((once_read = read(fd, dest, size)) != 0)
    {
        if(once_read > 0)
        {
            dest   =  (u8*)dest + once_read;
            size   -=  (size_t) once_read;
        }
        else if(errno != EINTR)
        {
            log_error("read(): %s (offset: %zu; size: %zu).",
                    strerror(errno), lseek(fd, 0, SEEK_CUR), size);
            return error;
        }
    }

    return ok;
}

static inline status set_file_offset(int fd, s64 offset, int whence)
{
    if(lseek(fd, offset, whence) != -1) return ok;

    // -------------------------- IN CASE OF ERROR --------------------------

    struct stat st;
    fstat(fd, &st);

    s64 real_offset;

    if(whence == SEEK_CUR) real_offset = lseek(fd, 0, SEEK_CUR) + offset;
    if(whence == SEEK_SET) real_offset = offset;
    if(whence == SEEK_END) real_offset = st.st_size + offset;

    log_error("lseek(): %s (offset: %jd).", strerror(errno), real_offset);

    return error;
}

#endif
