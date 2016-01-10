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

#ifndef READ_IMAGE_BLOCKS_H_INCLUDED
#define READ_IMAGE_BLOCKS_H_INCLUDED

#include "partclone.h"

struct instance
{
    struct {
        // the file descriptor of the partclone image
        int fd;
        // a number of the current block starting from 0
        u64 num;
        // the number of remaining bytes of this block
        u32 remaining_bytes;
        // is this block present in the image or does it remain unused?
        u8  existence;
        // no. of set blocks from the beggining of the image to this block
        u64 blocks_set;
        // the pointer to the last element of the bitmap
        u64 *bitmap_ptr;
        // which bit of the element of the bitmap represents this block?
        u8  bitmap_bit;
    } o;  // offset

    struct image *img;
};

status set_block(struct instance *obj, u64 block);
status next_block(struct instance *obj);
status offset_in_current_block(struct instance *obj, u64 offset);
status create_instance(struct instance *obj, struct image *img);
status close_instance(struct instance *obj);

#endif /* READ_IMAGE_BLOCKS_H_INCLUDED */
