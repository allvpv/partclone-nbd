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

#ifndef IMAGE_H_INCLUDED
#define IMAGE_H_INCLUDED

#include "partclone.h"

/* NONE - all blocks are used (like dd tool) */
#define BITMAP_NONE 0x00
/* BIT - classical bitmap */
#define BITMAP_BIT  0x01
/* BYTE - each byte corresponds to one block (especially used in 0001 images) */
#define BITMAP_BYTE 0x08

#define ENDIANNESS_COMPATIBLE   0xC0DE
#define ENDIANNESS_INCOMPATIBLE 0xDEC0

/* check bit, if bit is set, macro return 1, else 0 */
#define CHECK_BIT(var,pos) (!(!((var) & (1<<(pos)))))

enum image_version {v1, v2};
enum checksum_mode {crc32, ignore};
enum bitmap_mode   {bit = 0x01, byte = 0x08, none = 0x00};

struct image
{
    // ------------------------------ OFFSET -------------------------------

    // the file descriptor of the partclone image
    int fd;
    // a number of the current block starting from 0
    u64 o_num;
    // the number of remaining bytes of this block
    u32 o_remaining_bytes;
    // is this block present in the image or does it remain unused?
    u8  o_existence;
    // no. of set blocks from the beggining of the image to this block
    u64 o_blocks_set;
    // the pointer to the last element of the bitmap
    u64 *o_bitmap_ptr;
    // which bit of the element of the bitmap represents this block?
    u8  o_bitmap_bit;
 


    // ------------------------- BITMAP AND CACHE --------------------------

    // pointer to the first element of the bitmap
    u64 *bitmap_ptr;
    // size of the bitmap
    size_t bitmap_size;
    // bitmap elements (bitmap_size * 8)
    size_t bitmap_elements;
    // CRC32 or NONE
    enum bitmap_mode bmpmode;

    // number of blocks per one cache element
    u64 bitmap_elements_in_cache_element;
    // pointer to the bitmap cache
    u64 *cache_ptr;
    // size of the bitmap cache
    size_t cache_size;
    // cache elements (cache_size * 8)
    size_t cache_elements;

    // ---------------------------- PARAMETERS -----------------------------

    // file system path to an image
    char *path;
    // ENDIANNESS_COMPATIBLE (0xCODE) or ENDIANNESS_INCOMPATIBLE (0xDECO)
    u16 endianess_checker;
    // size of the source device in bytes
    u64 device_size;
    // number of blocks in the filesystem
    u64 blocks_count;
    // used blocks
	u64 used_blocks;
	// number of bytes in each block
    u32 block_size;
    // checksum algorithm used (CHECKSUM_CRC32 etc.)
	enum checksum_mode chkmode;
	// size of one checksum (ex. CHECKSUM_CRC32 - 4)
	u16 checksum_size;
	// how many blocks are checksumed together
	u32 blocks_per_checksum;
    // offset of data (from the beggining of the image)
    u64 data_offset;
    // offset of on-disk bitmap (from the beggining of the image)
    u64 bitmap_offset;
};

// initialization
status load_image(struct image *img, struct options *options);
status close_image(struct image *img);
status initialize_offset(struct image *img);

status set_block(struct image *obj, u64 block);
status next_block(struct image *obj);
status offset_in_current_block(struct image *obj, u64 offset);

#endif /* IMAGE_H_INCLUDED */
