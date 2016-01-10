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

#ifndef OPTIONS_H_INCLUDED
#define OPTIONS_H_INCLUDED

#include "partclone.h"


#define OPTION(name, type)                                                                                              \
struct {                                                                                                                \
    int set_by_user;                                                                                                    \
    type v;                                                                                                             \
} name

#define SET_OPTION(pointer, name, val) {                                                                                \
    pointer->name.v = (val);                                                                                            \
    pointer->name.set_by_user = 1;                                                                                      \
}


struct options {
    OPTION(image_path, char*);
//    OPTION(cfg_file, char*);
//    OPTION(auth_file, char*);
//    OPTION(pid_file, char*);
    OPTION(log_file, char*);
//    OPTION(max_connections, u64);
    OPTION(elems_per_cache, u64);
    OPTION(port, int);
//    OPTION(check_crc, int);
    OPTION(quiet, int);
//    OPTION(different_endianness, int);
//    OPTION(ip6, int);

    int arguments_left;
};

#endif // OPTIONS_H_INCLUDED
