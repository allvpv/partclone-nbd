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
#include "options.h"
#include "log.h"
#include "io.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include <string.h>

#include <stdlib.h>

#include <sys/mman.h>

struct new_header
{
    u8  magic[16]; // "partclone-image"
    u8  partclone_version_str[14]; // ex. "2.61"
    u8  image_version_str[4]; // "0002" (without terminating null)
    u16 endianess_checker; // ENDIANNESS_COMPATIBLE or ENDIANNESS_INCOMPATIBLE
    u8  fs_string[16]; // file system string
    u64 device_size; // size of source device in bytes
    u64 blocks_count; // number of blocks in the filesystem
    u64 used_blocks_filesystem; // used blocks (reported by filesystem)
    u64 used_blocks_bitmap; // used blocks (reported by bitmap)
    u32 block_size; // number of bytes in each block
    u32 feauture_size; // size of fields under bytes_per_block
    u16 image_version; // version of image (as in image_version_str)
    u16 cpu_bits; // partclone architecture (32-bit - 32, 64-bit - 64)
    u16 checksum_mode; // checksum algorithm used (CHECKSUM_CRC32 etc.)
    u16 checksum_size; // size of one checksum (ex. CHECKSUM_CRC32 - 4)
    u32 blocks_per_checksum; // how many blocks are checksumed
    u8  reseed_checksum; // reseed checksum after write (1 - yes, 0 - none)
    u8  bitmap_mode; // kind of bitmap (BM_BIT, BM_BYTE etc.)
    u32 crc32; // crc32 of fields above
} __attribute__ ((packed));

struct old_header
{
    u8  magic[15]; // "partclone-image" (without terminating null)
    u8  fs_string[15]; // file system string
    u8  image_version_str[4]; // "0001" (without terminating null)
    u8  padding[2];
    u32 block_size; // number of bytes in each block
    u64 device_size; // size of source device in bytes
    u64 blocks_count; // number of blocks in the filesystem
    u64 used_blocks; // used blocks
    u8  options[4096]; // not used
} __attribute__ ((packed));

union header
{
    struct old_header v1;
    struct new_header v2;
} __attribute__ ((packed));

static inline status load_byte_bitmap(struct image *img, int fd);
static status load_bit_bitmap(struct image *img, int fd);

status load_image(struct image *img, struct options *options)
{
    /* -------------------- OPEN IMAGE FILE -------------------- */

    img->path = options->image_path;

    int fd = open(img->path, O_RDONLY);

    if(fd == -1) {
        log_error("Cannot open image file: %s.", strerror(errno));
        goto error_1;
    } else {
        log_debug("Image file opened.");
    }

    /* -------------------- LOAD DATA FROM IMAGE HEADER -------------------- */

    union header head;

    if(read_whole(fd, &head, sizeof(union header)) == error) {
        log_error("Cannot read image header: %s.", strerror(errno));
        goto error_2;
    } else {
        log_debug("Image header readed.");
    }

    if(memcmp(head.v1.magic, "partclone-image", 15) != 0) {
        log_error("Incorrect image signature.");
        goto error_2;
    } else {
        log_debug("Correct image signature.");
    }


    if(memcmp(head.v1.image_version_str, "0001", 4) == 0) {

        log_debug("Detected supported image version 0001.");

        img->blocks_count = head.v1.blocks_count;
        img->block_size = head.v1.block_size;
        img->device_size = head.v1.device_size;
        img->used_blocks = head.v1.used_blocks;
        img->bitmap_offset = sizeof(struct old_header);
        img->data_offset = img->bitmap_offset + head.v1.blocks_count + 8;
        img->bitmap_elements_in_cache_element = options->elems_per_cache;

        /* 0001 images used corrupted checksum implementation - the first byte was
         * computed over and over again. To avoid a waste of time, checksum is
         * ignored.
         */

        img->chkmode = ignore;
        img->checksum_size = 4; /* CRC32 size */
        img->blocks_per_checksum = 1;
        img->bmpmode = byte;

        log_debug("Header data loaded.");


    } else if(memcmp(head.v2.image_version_str, "0002", 4) == 0) {

        log_debug("Detected supported image version 0002.");

        img->blocks_count = head.v2.blocks_count;
        img->blocks_per_checksum = head.v2.blocks_per_checksum;
        img->block_size = head.v2.block_size;
        img->bmpmode = head.v2.bitmap_mode;
        img->checksum_size = head.v2.checksum_size;
        img->device_size = head.v2.device_size;
        img->used_blocks = head.v2.used_blocks_bitmap;
        img->bitmap_offset = sizeof(struct new_header);
        img->bitmap_elements_in_cache_element = options->elems_per_cache;
        img->data_offset = img->bitmap_offset + divide_up(img->blocks_count, 8) + img->checksum_size;

        goto error_2;

    } else {

        log_error("Image version unsupported.");
        goto error_2;
    }

    /* -------------------- DISPLAY DATA FROM IMAGE HEADER -------------------- */

    log_debug("%s", "");
    log_debug("Information from header:");
    log_debug("- device size: " fu64 " bytes", img->device_size);
    log_debug("- blocks count: " fu64, img->blocks_count);
    log_debug("- used blocks: " fu64, img->used_blocks);
    log_debug("- block size: " fu32 " bytes", img->block_size);
    log_debug("- checksum size: " fu16 " bytes", img->checksum_size);
    log_debug("- blocks per checksum: " fu32, img->blocks_per_checksum);
    log_debug("%s", "");

    /* -------------------- LOAD BITMAP -------------------- */

    switch (img->bmpmode)
    {
    case bit:
        log_error("Bitmap type is \"bit\".");

        if(load_bit_bitmap(img, fd) == error) {
            goto error_2;
        }

        goto error_2;
    case none:
        log_error("Bitmap type \"none\" is not supported yet.");
        goto error_2;
    case byte:
        log_debug("Bitmap type is \"byte\".");

        if(load_byte_bitmap(img, fd) == error) {
            goto error_2;
        }

        break;
    default:
        log_error("Unsupported bitmap type.");
        goto error_2;
    }

    /* -------------------- LOAD CACHE -------------------- */

    img->cache_elements =
        divide_up(img->bitmap_elements, img->bitmap_elements_in_cache_element);

    img->cache_size = img->cache_elements * 8;

    log_debug("Memory required by cache array: " fu64 ".", img->cache_size);

    img->cache_ptr = calloc(img->cache_size, 1);

    if(img->cache_ptr == NULL) {
        log_error("Allocation failed.");
        goto error_3;
    } else {
        log_debug("Memory for bitmap cache allocated.");
    }

    u64 i, j;
    u64 *cache_ptr = img->cache_ptr;
    u64 *bitmap_ptr = img->bitmap_ptr;

    *cache_ptr++ = 0;

    for (i = 1; i < img->cache_elements; i++) {

        for (j = 0; j < img->bitmap_elements_in_cache_element; j++) {
            *cache_ptr += popcount(*bitmap_ptr++);
        }

        *(cache_ptr + 1) = *cache_ptr;
        cache_ptr++;
    }

    log_debug("Cache created.");


    log_info("Image loaded.");
    return ok;

    /* -------------------- ERROR HANDLING -------------------- */

error_3:

    free(img->bitmap_ptr);
    log_debug("Memory allocated by bitmap relased.");

error_2:

    if(close(fd) == -1) {
        log_error("Cannot close image file: %s", strerror(errno));
    } else {
        log_debug("Image file closed.");
    }

error_1:
    log_error("Cannot load image.");
    return error;
}

status close_image(struct image *img)
{
    free(img->cache_ptr);
    free(img->bitmap_ptr);

    log_debug("Memory allocated by bitmap cache released.");
    log_debug("Memory allocated by bitmap relased.");

    return ok;
}


static status load_byte_bitmap(struct image *img, int fd)
{
    /* allocate memory for bitmap */

    img->bitmap_elements = divide_up(img->blocks_count, 64);
    img->bitmap_size = img->bitmap_elements * 8;

    log_debug("Memory required by bitmap: " fu64 ".", img->bitmap_size);

    img->bitmap_ptr = calloc(img->bitmap_size, 1);

    if(img->bitmap_ptr == NULL) {
        log_error("Cannot allocate memory for bitmap.");
        goto error_1;
    } else {
        log_debug("Memory for bitmap allocated.");
    }

    /* -------------------- MAP BYTEMAP TO MEMORY -------------------- */

    u8 *bytearray_ptr;
    int length = img->blocks_count + 8 + img->bitmap_offset;

    bytearray_ptr = mmap(NULL, length, PROT_READ, MAP_SHARED, fd, 0);

    if(bytearray_ptr == MAP_FAILED) {
        log_error("Cannot map bytemap to memory: %s.", strerror(errno));
        goto error_2;
    }

    u8 *bytearray = bytearray_ptr + img->bitmap_offset;

    log_debug("Bytemap mapped to memory.");

    /* -------------------- CHECK BITMAP SIGNATURE -------------------- */

    if(memcmp(bytearray + img->blocks_count, "BiTmAgIc", 8) != 0) {
        log_error("Incorrect bitmap signature.");
        goto error_3;
    } else {
        log_debug("Correct bitmap signature.");
    }

    /* -------------------- CREATE BITMAP FROM BYTEMAP -------------------- */

    u64 i, first_iteration = img->blocks_count / 64,
           second_iteration = img->blocks_count - first_iteration * 64;

    u64 *bitmap_ptr = img->bitmap_ptr;

    #define t(x) *bitmap_ptr |= (u64) *bytearray++ << (x)

    for (i = 0; i < first_iteration; i++)
    {
        t(0);  t(1);  t(2);  t(3);  t(4);  t(5);  t(6);  t(7);  t(8);  t(9);  t(10); t(11); t(12); t(13); t(14); t(15);
        t(16); t(17); t(18); t(19); t(20); t(21); t(22); t(23); t(24); t(25); t(26); t(27); t(28); t(29); t(30); t(31);
        t(32); t(33); t(34); t(35); t(36); t(37); t(38); t(39); t(40); t(41); t(42); t(43); t(44); t(45); t(46); t(47);
        t(48); t(49); t(50); t(51); t(52); t(53); t(54); t(55); t(56); t(57); t(58); t(59); t(60); t(61); t(62); t(63);

        bitmap_ptr++;
    }

    #undef t

    int bit;

    /* -------------------- PROCEED REMAINING BLOCKS -------------------- */

    for (i = 0, bit = 0; i < second_iteration; i++, bit++) {
        *bitmap_ptr |= (u64) *bytearray++ << bit;
    }

    /*
     * end of bitmap is already filled with zeroes (calloc)
     */

    log_debug("Bytemap loaded to bitmap.");

    if(munmap(bytearray_ptr, length) == -1) {
        log_error("Cannot unmap bytemap: %s.", strerror(errno));
        goto error_1;
    } else {
        log_debug("Bytemap unmapped.");
    }

    log_debug("Bitmap created.");

    return ok;

    /* -------------------- ERROR HANDLING -------------------- */

error_3:

    if(munmap(bytearray_ptr, length) == -1) {
        log_error("Cannot unmap bytemap: %s.", strerror(errno));
    } else {
        log_debug("Bytemap unmapped.");
    }

error_2:

    free(img->bitmap_ptr);
    log_debug("Memory allocated by bitmap relased.");

error_1:

    log_error("Cannot load bytemap to bitmap.");
    return error;
}

static status load_bit_bitmap(struct image *img, int fd)
{
    /* allocate memory for bitmap */
    img->bitmap_elements = divide_up(img->blocks_count, 64);
    img->bitmap_size = img->bitmap_elements * 8;

    log_debug("Memory reqiuered by bitmap " fu64 " bytes.", img->bitmap_size);

    img->bitmap_ptr = calloc(img->bitmap_size, 1);

    if(img->bitmap_ptr == NULL) {
        log_error("Cannnot allocate memory for bitmap: %s.", strerror(errno));
        goto error_1;
    } else {
        log_debug("Memory for bitmap allocated.");
    }

    if(set_file_offset(fd, img->bitmap_offset, SEEK_SET) == error) {
        log_error("Cannot set file image offset to bitmap.");
        goto error_2;
    } else {
        log_debug("Offset set to bitmap.");
    }

    if(read_whole(fd, img->bitmap_ptr, divide_up(img->blocks_count, 8)) == error) {
        log_error("Cannot read bitmap: %s.", strerror(errno));
        goto error_2;
    }

   log_debug("Bitmap loaded.");

   return ok;

error_2:
   free(img->bitmap_ptr);
error_1:
   log_error("Cannot load bitmap.");
   return error;
}
