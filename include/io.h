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
            dest   +=  once_read;
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
