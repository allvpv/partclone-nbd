#ifndef CHECKSUM_H_INCLUDED
#define CHECKSUM_H_INCLUDED

#include "partclone.h"

#define CHECKSUM_NONE       0x00
#define CHECKSUM_CRC32      0x20
// Partclone images ver. 1 used defective CRC32 implementation. It doesn't
// increment buffer -- only the first byte was computed over and over again
#define CHECKSUM_DEFECTIVE  0xFF

void initialize_crc32(void);
u32 count_crc32(const void* data, size_t length, u32 previous_crc32);

#endif // CHECKSUM_H_INCLUDED
