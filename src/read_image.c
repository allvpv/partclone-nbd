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

#include "partclone.h"
#include "log.h"
#include "read_image.h"
#include "io.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include <string.h>

static inline status set_block_from_the_ground(struct instance *obj, u64 block);

status create_instance(struct instance *obj, struct image *img)
{
    obj->img = img;

    obj->o.num = 0;
    obj->o.bitmap_bit = 0;
    obj->o.bitmap_ptr = obj->img->bitmap_ptr;
    obj->o.blocks_set = 0;
    obj->o.existence = *obj->img->bitmap_ptr & 1;
    obj->o.remaining_bytes = obj->img->block_size;

    obj->o.fd = open(obj->img->path, O_RDONLY) ;

    if(obj->o.fd == -1) {
        log_error("Cannot open image file for reading.");
        goto error;
    }

    if(set_file_offset(obj->o.fd, obj->img->data_offset, SEEK_SET) == error) {
        close(obj->o.fd);
    }

    log_debug("Image instance created.");

    return ok;

error:
    log_error("Cannot create an image instance.");
    return error;
}

status close_instance(struct instance *obj)
{
    if(close(obj->o.fd) == -1) {
        log_error("Cannot close an image file: %s.", strerror(errno));
        return error;
    } else {
        log_debug("Image file closed.");
    }

    return ok;
}


status set_block(struct instance *obj, u64 block)
{
    if(block == obj->o.num + 1) {
        if(next_block(obj) == error) goto error;

    } else if(block != obj->o.num) {
        if(set_block_from_the_ground(obj, block) == error) goto error;

    } else {
        if(offset_in_current_block(obj, 0) == error) goto error;
    }

    return ok;

    error: {
        log_error("Cannot set offset in " fu64 " block.", block);
        return error;
    }
}

static inline status set_block_from_the_ground(struct instance *obj, u64 block)
{
    u64 i;

    /* (a) compute basic values
     * (a) assign calculated data to the image
     * (b) find bitmap element corresponding to this block
     * (c) is the block present in the image
     * (d) find bitmap cache element corresponding to this block
     * (e) how many blocks are not included in finded bitmap cache element
     * (f) how many iterations are required
     * (g) count bits from 64 bit elements
     * (h) set bitmap pointer on the bitmap cache element
     * (i) how many bits are not included in the last iteration - create a mask
     *     for the last bitmap element
     * (j) add remaining bits by masking the current element using mask computed
     *     in point (g)
     * (k) compute offset
     */

    /* (a) ----------------------------------------------------------------- */
    obj->o.num = block;
    obj->o.remaining_bytes = obj->img->block_size;

    /* (b) ----------------------------------------------------------------- */
    u64 bitmap_element = obj->o.num / 64;
    obj->o.bitmap_ptr = obj->img->bitmap_ptr + bitmap_element;
    obj->o.bitmap_bit = obj->o.num % 64;

    /* (c) ----------------------------------------------------------------- */
    obj->o.existence = (*obj->o.bitmap_ptr >> obj->o.bitmap_bit) & 1;

    /* (d) ----------------------------------------------------------------- */

//    u64 cache_element = 0;
//    obj->o.blocks_set = 0;

    u64 cache_element =
        bitmap_element / obj->img->bitmap_elements_in_cache_element;

    obj->o.blocks_set = obj->img->cache_ptr[cache_element];

    /* (e) ----------------------------------------------------------------- */
    u64 remaining_blocks =
        obj->o.num - cache_element *
        obj->img->bitmap_elements_in_cache_element * 64;

    /* (f) ----------------------------------------------------------------- */
    u64 iterations_num = remaining_blocks / 64;

    /* (g) ----------------------------------------------------------------- */
    u64 *bitmap_ptr =
        obj->img->bitmap_ptr + cache_element *
        obj->img->bitmap_elements_in_cache_element;

    /* (h) ----------------------------------------------------------------- */
    for (i = 0; i < iterations_num; i++) {
        obj->o.blocks_set += popcount(*bitmap_ptr++);
    }

    /* (i) ----------------------------------------------------------------- */

    /* Why not 0xFFFFFFFFFFFFFFFF >> 64? This trick is necessary, because
     * shifting a 64-bit variable 64 bits is in fact shifting 0 bits.
     */

    u64 shifter = 64 - (remaining_blocks - iterations_num * 64);

    u64 last_bitmap_elem_mask =
        (0xFFFFFFFFFFFFFFFF >> shifter) & (-(shifter < 64));

    /* (j) ----------------------------------------------------------------- */
    obj->o.blocks_set += popcount(*bitmap_ptr & last_bitmap_elem_mask);

    /* (h) ----------------------------------------------------------------- */
    u64 file_offset =
        obj->o.blocks_set * obj->img->block_size
        + (obj->o.blocks_set / obj->img->blocks_per_checksum) * obj->img->checksum_size
        + obj->img->data_offset;

    if(set_file_offset(obj->o.fd, file_offset, SEEK_SET) == error) {
        return error;
    }

    return ok;
}

status next_block(struct instance *obj)
{
    /* (1) is the next element of bitmap in a new byte, or in an old byte?
     * (2) compute addional offset when jumping to next block
     * (3) is the block present in the image?
     * (4) change the total number of set blocks
     * (5) reset remaining bytes number
     * (6) jump to next block
     */


    /* (1) ----------------------------------------------------------------- */

    u8 next_bit_raw = obj->o.bitmap_bit + 1;

    obj->o.bitmap_bit = next_bit_raw % 64;
    obj->o.bitmap_ptr += (next_bit_raw - 63) * (next_bit_raw / 63);
    obj->o.num++;

    /* (2) ----------------------------------------------------------------- */

    u64 addional_offset = obj->o.existence *
    (((((obj->o.blocks_set + 1) % obj->img->blocks_per_checksum) == 0) * obj->img->checksum_size) + obj->o.remaining_bytes);

    /* (3) ----------------------------------------------------------------- */

    obj->o.existence =
        (*obj->o.bitmap_ptr >> obj->o.bitmap_bit) & 1;

    /* (4) ----------------------------------------------------------------- */

    obj->o.blocks_set += obj->o.existence;

    /* (5) ----------------------------------------------------------------- */

    obj->o.remaining_bytes = obj->img->block_size;

    /* (6) ----------------------------------------------------------------- */

    if(set_file_offset(obj->o.fd, addional_offset, SEEK_CUR) == error) {
        log_error("Cannot set offset in next block.");
        return error;
    }

    return ok;
}

/* set a new offset inside a current block */
status offset_in_current_block(struct instance *obj, u64 offset)
{
    u32 blk_remaining = obj->img->block_size - offset;

    if(set_file_offset(obj->o.fd, obj->o.remaining_bytes - blk_remaining,
                        SEEK_CUR) == error) {
        log_error("Cannot set offset inside block.");
        return error;
    }

    obj->o.remaining_bytes = blk_remaining;
    return ok;
}
