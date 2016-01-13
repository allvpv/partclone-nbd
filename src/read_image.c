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
