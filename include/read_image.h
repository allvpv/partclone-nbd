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
