/***************************************************************************

NeoGeo 'C' ROM encryption

Starting with KOF99, all NeoGeo games have encrypted graphics. Additionally
to that, the data for the front text layer, which was previously stored in
a separate ROM, is stored at the end of the tile data.

The encryption is one of the nastiest implementation of a XOR scheme ever
seen, involving 9 seemingly uncorrelated 256-byte tables. All known games use
the same tables except KOF2000 and MS4 which use a different set.

The 32 data bits of every longword are decrypted in a single step (one byte at
a time), but the values to use for the xor are determined in a convoluted way.
It's actually so convoluted that it's too difficult to describe - please refer
to the source below.
Suffice to say that bytes are handled in couples (0&3 and 1&2), and the two xor
values are taken from three tables, the indexes inside the tables depending on
bits 0-7 and 8-15 of the address, in one case further xored through the table
used in step 5) below. Additionally, the bytes in a couple can be swapped,
depending either on bit 8 of the address, or on bit 16 xored with the table
used in step 4) below.

The 24 address bits are encrypted in five steps. Each step xors 8 bits with a
value taken from a different table; the index inside the table depends on 8
other bits.
0) xor bits  0-7  with a fixed value that changes from game to game
1) xor bits  8-15 depending on bits 16-23
2) xor bits  8-15 depending on bits  0-7
3) xor bits 16-23 depending on bits  0-7
4) xor bits 16-23 depending on bits  8-15
5) xor bits  0-7  depending on bits  8-15

Each step acts on the current value, so e.g. step 4) uses bits 8-15 as modified
by step 2).

[Note: the table used in step 1) is currently incomplete due to lack of data to
analyze]


There are two major weaknesses in this encryption algorithm, that exposed it to
a known plaintext attack.

The first weakness is that the data xor depends on the address inside the
encrypted ROM instead that on the decrypted address; together with the high
concentration of 0x00 and 0xFF in the decrypted data (more than 60% of the
total), this exposed easily recognizable patterns in the encrypted data, which
could be exploited with some simple statistical checks. The deviousness of the
xor scheme was the major difficulty.

The second weakness is that the address scrambling works on 32-bit words. Since
there are a large number of 32-bit values that appear only once in the whole
encrypted ROM space, this means that once the xor layer was broken, a large
table of encrypted-decrypted address correspondencies could be built and
analyzed, quickly leading to the algorithm.

***************************************************************************/

#include "driver.h"
#include "neogeo.h"


const unsigned char *type0_t03;
const unsigned char *type0_t12;
const unsigned char *type1_t03;
const unsigned char *type1_t12;
const unsigned char *address_8_15_xor1;
const unsigned char *address_8_15_xor2;
const unsigned char *address_16_23_xor1;
const unsigned char *address_16_23_xor2;
const unsigned char *address_0_7_xor;


const unsigned char kof99_type0_t03[256] =
{
	0xfb, 0x86, 0x9d, 0xf1, 0xbf, 0x80, 0xd5, 0x43, 0xab, 0xb3, 0x9f, 0x6a, 0x33, 0xd9, 0xdb, 0xb6,
	0x66, 0x08, 0x69, 0x88, 0xcc, 0xb7, 0xde, 0x49, 0x97, 0x64, 0x1f, 0xa6, 0xc0, 0x2f, 0x52, 0x42,
	0x44, 0x5a, 0xf2, 0x28, 0x98, 0x87, 0x96, 0x8a, 0x83, 0x0b, 0x03, 0x61, 0x71, 0x99, 0x6b, 0xb5,
	0x1a, 0x8e, 0xfe, 0x04, 0xe1, 0xf7, 0x7d, 0xdd, 0xed, 0xca, 0x37, 0xfc, 0xef, 0x39, 0x72, 0xda,
	0xb8, 0xbe, 0xee, 0x7f, 0xe5, 0x31, 0x78, 0xf3, 0x91, 0x9a, 0xd2, 0x11, 0x19, 0xb9, 0x09, 0x4c,
	0xfd, 0x6d, 0x2a, 0x4d, 0x65, 0xa1, 0x89, 0xc7, 0x75, 0x50, 0x21, 0xfa, 0x16, 0x00, 0xe9, 0x12,
	0x74, 0x2b, 0x1e, 0x4f, 0x14, 0x01, 0x70, 0x3a, 0x4e, 0x3f, 0xf5, 0xf4, 0x1d, 0x3d, 0x15, 0x27,
	0xa7, 0xff, 0x45, 0xe0, 0x6e, 0xf9, 0x54, 0xc8, 0x48, 0xad, 0xa5, 0x0a, 0xf6, 0x2d, 0x2c, 0xe2,
	0x68, 0x67, 0xd6, 0x85, 0xb4, 0xc3, 0x34, 0xbc, 0x62, 0xd3, 0x5f, 0x84, 0x06, 0x5b, 0x0d, 0x95,
	0xea, 0x5e, 0x9e, 0xd4, 0xeb, 0x90, 0x7a, 0x05, 0x81, 0x57, 0xe8, 0x60, 0x2e, 0x20, 0x25, 0x7c,
	0x46, 0x0c, 0x93, 0xcb, 0xbd, 0x17, 0x7e, 0xec, 0x79, 0xb2, 0xc2, 0x22, 0x41, 0xb1, 0x10, 0xac,
	0xa8, 0xbb, 0x9b, 0x82, 0x4b, 0x9c, 0x8b, 0x07, 0x47, 0x35, 0x24, 0x56, 0x8d, 0xaf, 0xe6, 0x26,
	0x40, 0x38, 0xc4, 0x5d, 0x1b, 0xc5, 0xd1, 0x0f, 0x6c, 0x7b, 0xb0, 0xe3, 0xa3, 0x23, 0x6f, 0x58,
	0xc1, 0xba, 0xcf, 0xd7, 0xa2, 0xe7, 0xd0, 0x63, 0x5c, 0xf8, 0x73, 0xa0, 0x13, 0xdc, 0x29, 0xcd,
	0xc9, 0x76, 0xae, 0x8f, 0xe4, 0x59, 0x30, 0xaa, 0x94, 0x1c, 0x3c, 0x0e, 0x55, 0x92, 0x77, 0x32,
	0xc6, 0xce, 0x18, 0x36, 0xdf, 0xa9, 0x8c, 0xd8, 0xa4, 0xf0, 0x3b, 0x51, 0x4a, 0x02, 0x3e, 0x53,
};

const unsigned char kof99_type0_t12[256] =
{
	0x1f, 0xac, 0x4d, 0xcd, 0xca, 0x70, 0x02, 0x6b, 0x18, 0x40, 0x62, 0xb2, 0x3f, 0x9b, 0x5b, 0xef,
	0x69, 0x68, 0x71, 0x3b, 0xcb, 0xd4, 0x30, 0xbc, 0x47, 0x72, 0x74, 0x5e, 0x84, 0x4c, 0x1b, 0xdb,
	0x6a, 0x35, 0x1d, 0xf5, 0xa1, 0xb3, 0x87, 0x5d, 0x57, 0x28, 0x2f, 0xc4, 0xfd, 0x24, 0x26, 0x36,
	0xad, 0xbe, 0x61, 0x63, 0x73, 0xaa, 0x82, 0xee, 0x29, 0xd0, 0xdf, 0x8c, 0x15, 0xb5, 0x96, 0xf3,
	0xdd, 0x7e, 0x3a, 0x37, 0x58, 0x7f, 0x0c, 0xfc, 0x0b, 0x07, 0xe8, 0xf7, 0xf4, 0x14, 0xb8, 0x81,
	0xb6, 0xd7, 0x1e, 0xc8, 0x85, 0xe6, 0x9d, 0x33, 0x60, 0xc5, 0x95, 0xd5, 0x55, 0x00, 0xa3, 0xb7,
	0x7d, 0x50, 0x0d, 0xd2, 0xc1, 0x12, 0xe5, 0xed, 0xd8, 0xa4, 0x9c, 0x8f, 0x2a, 0x4f, 0xa8, 0x01,
	0x52, 0x83, 0x65, 0xea, 0x9a, 0x6c, 0x44, 0x4a, 0xe2, 0xa5, 0x2b, 0x46, 0xe1, 0x34, 0x25, 0xf8,
	0xc3, 0xda, 0xc7, 0x6e, 0x48, 0x38, 0x7c, 0x78, 0x06, 0x53, 0x64, 0x16, 0x98, 0x3c, 0x91, 0x42,
	0x39, 0xcc, 0xb0, 0xf1, 0xeb, 0x13, 0xbb, 0x05, 0x32, 0x86, 0x0e, 0xa2, 0x0a, 0x9e, 0xfa, 0x66,
	0x54, 0x8e, 0xd3, 0xe7, 0x19, 0x20, 0x77, 0xec, 0xff, 0xbd, 0x6d, 0x43, 0x23, 0x03, 0xab, 0x75,
	0x3d, 0xcf, 0xd1, 0xde, 0x92, 0x31, 0xa7, 0x45, 0x4b, 0xc2, 0x97, 0xf9, 0x7a, 0x88, 0xd9, 0x1c,
	0xe9, 0xe4, 0x10, 0xc9, 0x22, 0x2d, 0x90, 0x76, 0x17, 0x79, 0x04, 0x51, 0x1a, 0x5a, 0x5f, 0x2c,
	0x21, 0x6f, 0x3e, 0xe0, 0xf0, 0xbf, 0xd6, 0x94, 0x0f, 0x80, 0x11, 0xa0, 0x5c, 0xa9, 0x49, 0x2e,
	0xce, 0xaf, 0xa6, 0x9f, 0x7b, 0x99, 0xb9, 0xb4, 0xe3, 0xfb, 0xf6, 0x27, 0xf2, 0x93, 0xfe, 0x08,
	0x67, 0xae, 0x09, 0x89, 0xdc, 0x4e, 0xc6, 0xc0, 0x8a, 0xb1, 0x59, 0x8b, 0x41, 0x56, 0x8d, 0xba,
};

const unsigned char kof99_type1_t03[256] =
{
	0xa9, 0x17, 0xaf, 0x0d, 0x34, 0x6e, 0x53, 0xb6, 0x7f, 0x58, 0xe9, 0x14, 0x5f, 0x55, 0xdb, 0xd4,
	0x42, 0x80, 0x99, 0x59, 0xa8, 0x3a, 0x57, 0x5d, 0xd5, 0x6f, 0x4c, 0x68, 0x35, 0x46, 0xa6, 0xe7,
	0x7b, 0x71, 0xe0, 0x93, 0xa2, 0x1f, 0x64, 0x21, 0xe3, 0xb1, 0x98, 0x26, 0xab, 0xad, 0xee, 0xe5,
	0xbb, 0xd9, 0x1e, 0x2e, 0x95, 0x36, 0xef, 0x23, 0x79, 0x45, 0x04, 0xed, 0x13, 0x1d, 0xf4, 0x85,
	0x96, 0xec, 0xc2, 0x32, 0xaa, 0x7c, 0x15, 0xd8, 0xda, 0x92, 0x90, 0x9d, 0xb7, 0x56, 0x6a, 0x66,
	0x41, 0xfc, 0x00, 0xf6, 0x50, 0x24, 0xcf, 0xfb, 0x11, 0xfe, 0x82, 0x48, 0x9b, 0x27, 0x1b, 0x67,
	0x4e, 0x84, 0x69, 0x97, 0x6d, 0x8c, 0xd2, 0xba, 0x74, 0xf9, 0x8f, 0xa5, 0x54, 0x5c, 0xcd, 0x73,
	0x07, 0xd1, 0x01, 0x09, 0xf1, 0x19, 0x3b, 0x5e, 0x87, 0x30, 0x76, 0xcc, 0xc0, 0x5a, 0xa7, 0x49,
	0x22, 0xfa, 0x16, 0x02, 0xdf, 0xa4, 0xff, 0xb3, 0x75, 0x33, 0xbd, 0x88, 0x2f, 0xcb, 0x2a, 0x44,
	0xb8, 0xbf, 0x1c, 0x0f, 0x81, 0x10, 0x43, 0xb4, 0xc8, 0x7e, 0x9a, 0x25, 0xea, 0x83, 0x4b, 0x38,
	0x7a, 0xd7, 0x3d, 0x1a, 0x4f, 0x62, 0x51, 0xc9, 0x47, 0x0e, 0xce, 0x3f, 0xc7, 0x4d, 0x2c, 0xa1,
	0x86, 0xb9, 0xc5, 0xca, 0xdd, 0x6b, 0x70, 0x6c, 0x91, 0x9c, 0xbe, 0x0a, 0x9f, 0xf5, 0x94, 0xbc,
	0x18, 0x2b, 0x60, 0x20, 0x29, 0xf7, 0xf2, 0x28, 0xc4, 0xa0, 0x0b, 0x65, 0xde, 0x8d, 0x78, 0x12,
	0x3e, 0xd0, 0x77, 0x08, 0x8b, 0xae, 0x05, 0x31, 0x3c, 0xd6, 0xa3, 0x89, 0x06, 0xdc, 0x52, 0x72,
	0xb0, 0xb5, 0x37, 0xd3, 0xc3, 0x8a, 0xc6, 0xf0, 0xc1, 0x61, 0xfd, 0x4a, 0x5b, 0x7d, 0x9e, 0xf3,
	0x63, 0x40, 0x2d, 0xe8, 0xb2, 0xe6, 0x39, 0x03, 0xeb, 0x8e, 0xe1, 0x0c, 0xe4, 0xe2, 0xf8, 0xac,
};

const unsigned char kof99_type1_t12[256] =
{
	0xea, 0xe6, 0x5e, 0xa7, 0x8e, 0xac, 0x34, 0x03, 0x30, 0x97, 0x52, 0x53, 0x76, 0xf2, 0x62, 0x0b,
	0x0a, 0xfc, 0x94, 0xb8, 0x67, 0x36, 0x11, 0xbc, 0xae, 0xca, 0xfa, 0x15, 0x04, 0x2b, 0x17, 0xc4,
	0x3e, 0x5b, 0x59, 0x01, 0x57, 0xe2, 0xba, 0xb7, 0xd1, 0x3f, 0xf0, 0x6a, 0x9c, 0x2a, 0xcb, 0xa9,
	0xe3, 0x2c, 0xc0, 0x0f, 0x46, 0x91, 0x8a, 0xd0, 0x98, 0xc5, 0xa6, 0x1b, 0x96, 0x29, 0x12, 0x09,
	0x63, 0xed, 0xe0, 0xa2, 0x86, 0x77, 0xbe, 0xe5, 0x65, 0xdb, 0xbd, 0x50, 0xb3, 0x9d, 0x1a, 0x4e,
	0x79, 0x0c, 0x00, 0x43, 0xdf, 0x3d, 0x54, 0x33, 0x8f, 0x89, 0xa8, 0x7b, 0xf9, 0xd5, 0x27, 0x82,
	0xbb, 0xc2, 0x8c, 0x47, 0x88, 0x6b, 0xb4, 0xc3, 0xf8, 0xaa, 0x06, 0x1e, 0x83, 0x7d, 0x05, 0x78,
	0x85, 0xf6, 0x6e, 0x2e, 0xec, 0x5a, 0x31, 0x45, 0x38, 0x14, 0x16, 0x8b, 0x02, 0xe4, 0x4f, 0xb0,
	0xbf, 0xab, 0xa4, 0x9e, 0x48, 0x60, 0x19, 0x35, 0x08, 0xde, 0xdd, 0x66, 0x90, 0x51, 0xcc, 0xa3,
	0xaf, 0x70, 0x9b, 0x75, 0x95, 0x49, 0x6c, 0x64, 0x72, 0x7e, 0x44, 0xa0, 0x73, 0x25, 0x68, 0x55,
	0x1f, 0x40, 0x7a, 0x74, 0x0e, 0x8d, 0xdc, 0x1c, 0x71, 0xc8, 0xcf, 0xd7, 0xe8, 0xce, 0xeb, 0x32,
	0x3a, 0xee, 0x07, 0x61, 0x4d, 0xfe, 0x5c, 0x7c, 0x56, 0x2f, 0x2d, 0x5f, 0x6f, 0x9f, 0x81, 0x22,
	0x58, 0x4b, 0xad, 0xda, 0xb9, 0x10, 0x18, 0x23, 0xe1, 0xf3, 0x6d, 0xe7, 0xe9, 0x28, 0xd6, 0xd8,
	0xf4, 0x4c, 0x39, 0x21, 0xb2, 0x84, 0xc1, 0x24, 0x26, 0xf1, 0x93, 0x37, 0xc6, 0x4a, 0xcd, 0x20,
	0xc9, 0xd9, 0xc7, 0xb1, 0xff, 0x99, 0xd4, 0x5d, 0xb5, 0xa1, 0x87, 0x0d, 0x69, 0x92, 0x13, 0x80,
	0xd2, 0xd3, 0xfd, 0x1d, 0xf5, 0x3b, 0xa5, 0x7f, 0xef, 0x9a, 0xb6, 0x42, 0xfb, 0x3c, 0xf7, 0x41,
};



/* underlined values are wrong (not enough evidence, FF fill in kof99 and garou) */
/* they correspond to tiles 7d000-7efff */
const unsigned char kof99_address_8_15_xor1[256] =
{
	0x00, 0xb1, 0x1e, 0xc5, 0x3d, 0x40, 0x45, 0x5e, 0xf2, 0xf8, 0x04, 0x63, 0x36, 0x87, 0x88, 0xbf,
	0xab, 0xcc, 0x78, 0x08, 0xdd, 0x20, 0xd4, 0x35, 0x09, 0x8e, 0x44, 0xae, 0x33, 0xa9, 0x9e, 0xcd,
	0xb3, 0xe5, 0xad, 0x41, 0xda, 0xbe, 0xf4, 0x16, 0x57, 0x2e, 0x53, 0x67, 0xaf, 0xdb, 0x8a, 0xd8,
	0x34, 0x17, 0x3c, 0x01, 0x55, 0x73, 0xcf, 0xe3, 0xe8, 0xc7, 0x0d, 0xe9, 0xa3, 0x13, 0x0c, 0xf6,
	0x90, 0x4e, 0xfb, 0x97, 0x6d, 0x5f, 0xa8, 0x71, 0x11, 0xfc, 0xd1, 0x95, 0x81, 0xba, 0x8c, 0x1b,
	0x39, 0xfe, 0xa2, 0x15, 0xa6, 0x52, 0x4d, 0x5b, 0x59, 0xa5, 0xe0, 0x96, 0xd9, 0x8f, 0x7b, 0xed,
	0x29, 0xd3, 0x1f, 0x0e, 0xec, 0x23, 0x0f, 0xb8, 0x6c, 0x6f, 0x7d, 0x18, 0x46, 0xd6, 0xe4, 0xb5,
	0x9a, 0x79, 0x02, 0xf5, 0x03, 0xc0, 0x60, 0x66, 0x5c, 0x2f, 0x76, 0x85, 0x9d, 0x54, 0x1a, 0x6a,
	0x28, 0xce, 0x7f, 0x7c, 0x91, 0x99, 0x4c, 0x83, 0x3e, 0xb4, 0x1d, 0x05, 0xc1, 0xc3, 0xd7, 0x47,
	0xde, 0xbc, 0x62, 0x6e, 0x86, 0x14, 0x80, 0x77, 0xeb, 0xf3, 0x07, 0x31, 0x56, 0xd2, 0xc2, 0xc6,
	0x6b, 0xdc, 0xfd, 0x22, 0x92, 0xf0, 0x06, 0x51, 0x2d, 0x38, 0xe6, 0xa0, 0x25, 0xdf, 0xd5, 0x2c,
	0x1c, 0x94, 0x12, 0x9c, 0xb0, 0x9b, 0xc4, 0x0b, 0xc8, 0xd0, 0xf7, 0x30, 0xcb, 0x27, 0xfa, 0x7a,
	0x10, 0x61, 0xaa, 0xa4, 0x70, 0xb7, 0x2a, 0x5a, 0xc9, 0xf1, 0x0a, 0x49, 0x65, 0xee, 0x69, 0x4b,
	0x3a, 0x8d, 0x32, 0x5d, 0x68, 0xb9, 0x9f, 0x75, 0x19, 0x3f, 0xac, 0x37, 0x4f, 0xe7, 0x93, 0x89,
	0x7e, 0x4a, 0x3b, 0xea, 0x74, 0x72, 0x43, 0xbd, 0x24, 0xef, 0xb6, 0xff, 0x64, 0x58, 0x84, 0x8b,
	0xa7, 0xbb, 0xb2, 0xe1, 0x26, 0x2b, 0x50, 0xca, 0x21, 0xf9, 0x98, 0xa1, 0xe2, 0x42, 0x82, 0x48,
/*	                                                            ^^^^  ^^^^  ^^^^  ^^^^*/
};

const unsigned char kof99_address_8_15_xor2[256] =
{
	0x9b, 0x9d, 0xc1, 0x3d, 0xa9, 0xb8, 0xf4, 0x6f, 0xf6, 0x25, 0xc7, 0x47, 0xd5, 0x97, 0xdf, 0x6b,
	0xeb, 0x90, 0xa4, 0xb2, 0x5d, 0xf5, 0x66, 0xb0, 0xb9, 0x8b, 0x93, 0x64, 0xec, 0x7b, 0x65, 0x8c,
	0xf1, 0x43, 0x42, 0x6e, 0x45, 0x9f, 0xb3, 0x35, 0x06, 0x71, 0x96, 0xdb, 0xa0, 0xfb, 0x0b, 0x3a,
	0x1f, 0xf8, 0x8e, 0x69, 0xcd, 0x26, 0xab, 0x86, 0xa2, 0x0c, 0xbd, 0x63, 0xa5, 0x7a, 0xe7, 0x6a,
	0x5f, 0x18, 0x9e, 0xbf, 0xad, 0x55, 0xb1, 0x1c, 0x5c, 0x03, 0x30, 0xc6, 0x37, 0x20, 0xe3, 0xc9,
	0x52, 0xe8, 0xee, 0x4f, 0x01, 0x70, 0xc4, 0x77, 0x29, 0x2a, 0xba, 0x53, 0x12, 0x04, 0x7d, 0xaf,
	0x33, 0x8f, 0xa8, 0x4d, 0xaa, 0x5b, 0xb4, 0x0f, 0x92, 0xbb, 0xed, 0xe1, 0x2f, 0x50, 0x6c, 0xd2,
	0x2c, 0x95, 0xd9, 0xf9, 0x98, 0xc3, 0x76, 0x4c, 0xf2, 0xe4, 0xe5, 0x2b, 0xef, 0x9c, 0x49, 0xb6,
	0x31, 0x3b, 0xbc, 0xa1, 0xca, 0xde, 0x62, 0x74, 0xea, 0x81, 0x00, 0xdd, 0xa6, 0x46, 0x88, 0x3f,
	0x39, 0xd6, 0x23, 0x54, 0x24, 0x4a, 0xd8, 0xdc, 0xd7, 0xd1, 0xcc, 0xbe, 0x57, 0x7c, 0xda, 0x44,
	0x61, 0xce, 0xd3, 0xd4, 0xe9, 0x28, 0x80, 0xe0, 0x56, 0x8a, 0x09, 0x05, 0x9a, 0x89, 0x1b, 0xf7,
	0xf3, 0x99, 0x6d, 0x5e, 0x48, 0x91, 0xc0, 0xd0, 0xc5, 0x79, 0x78, 0x41, 0x59, 0x21, 0x2e, 0xff,
	0xc2, 0x4b, 0x38, 0x83, 0x32, 0xe6, 0xe2, 0x7f, 0x1e, 0x17, 0x58, 0x1d, 0x1a, 0xfa, 0x85, 0x82,
	0x94, 0xc8, 0x72, 0x7e, 0xb7, 0xac, 0x0e, 0xfc, 0xfd, 0x16, 0x27, 0x75, 0x8d, 0xcb, 0x08, 0xfe,
	0x0a, 0x02, 0x0d, 0x36, 0x11, 0x22, 0x84, 0x40, 0x34, 0x3e, 0x2d, 0x68, 0x5a, 0xa7, 0x67, 0xae,
	0x87, 0x07, 0x10, 0x60, 0x14, 0x73, 0x3c, 0x51, 0x19, 0xa3, 0xb5, 0xcf, 0x13, 0xf0, 0x15, 0x4e,
};

const unsigned char kof99_address_16_23_xor1[256] =
{
	0x00, 0x5f, 0x03, 0x52, 0xce, 0xe3, 0x7d, 0x8f, 0x6b, 0xf8, 0x20, 0xde, 0x7b, 0x7e, 0x39, 0xbe,
	0xf5, 0x94, 0x18, 0x78, 0x80, 0xc9, 0x7f, 0x7a, 0x3e, 0x63, 0xf2, 0xe0, 0x4e, 0xf7, 0x87, 0x27,
	0x69, 0x6c, 0xa4, 0x1d, 0x85, 0x5b, 0xe6, 0x44, 0x25, 0x0c, 0x98, 0xc7, 0x01, 0x02, 0xa3, 0x26,
	0x09, 0x38, 0xdb, 0xc3, 0x1e, 0xcf, 0x23, 0x45, 0x68, 0x76, 0xd6, 0x22, 0x5d, 0x5a, 0xae, 0x16,
	0x9f, 0xa2, 0xb5, 0xcd, 0x81, 0xea, 0x5e, 0xb8, 0xb9, 0x9d, 0x9c, 0x1a, 0x0f, 0xff, 0xe1, 0xe7,
	0x74, 0xaa, 0xd4, 0xaf, 0xfc, 0xc6, 0x33, 0x29, 0x5c, 0xab, 0x95, 0xf0, 0x19, 0x47, 0x59, 0x67,
	0xf3, 0x96, 0x60, 0x1f, 0x62, 0x92, 0xbd, 0x89, 0xee, 0x28, 0x13, 0x06, 0xfe, 0xfa, 0x32, 0x6d,
	0x57, 0x3c, 0x54, 0x50, 0x2c, 0x58, 0x49, 0xfb, 0x17, 0xcc, 0xef, 0xb2, 0xb4, 0xf9, 0x07, 0x70,
	0xc5, 0xa9, 0xdf, 0xd5, 0x3b, 0x86, 0x2b, 0x0d, 0x6e, 0x4d, 0x0a, 0x90, 0x43, 0x31, 0xc1, 0xf6,
	0x88, 0x0b, 0xda, 0x53, 0x14, 0xdc, 0x75, 0x8e, 0xb0, 0xeb, 0x99, 0x46, 0xa1, 0x15, 0x71, 0xc8,
	0xe9, 0x3f, 0x4a, 0xd9, 0x73, 0xe5, 0x7c, 0x30, 0x77, 0xd3, 0xb3, 0x4b, 0x37, 0x72, 0xc2, 0x04,
	0x97, 0x08, 0x36, 0xb1, 0x3a, 0x61, 0xec, 0xe2, 0x1c, 0x9a, 0x8b, 0xd1, 0x1b, 0x2e, 0x9e, 0x8a,
	0xd8, 0x41, 0xe4, 0xc4, 0x40, 0x2f, 0xad, 0xc0, 0xb6, 0x84, 0x51, 0x66, 0xbb, 0x12, 0xe8, 0xdd,
	0xcb, 0xbc, 0x6f, 0xd0, 0x11, 0x83, 0x56, 0x4c, 0xca, 0xbf, 0x05, 0x10, 0xd7, 0xba, 0xfd, 0xed,
	0x8c, 0x0e, 0x4f, 0x3d, 0x35, 0x91, 0xb7, 0xac, 0x34, 0x64, 0x2a, 0xf1, 0x79, 0x6a, 0x9b, 0x2d,
	0x65, 0xf4, 0x42, 0xa0, 0x8d, 0xa7, 0x48, 0x55, 0x21, 0x93, 0x24, 0xd2, 0xa6, 0xa5, 0xa8, 0x82,
};

const unsigned char kof99_address_16_23_xor2[256] =
{
	0x29, 0x97, 0x1a, 0x2c, 0x0b, 0x94, 0x3e, 0x75, 0x01, 0x0d, 0x1b, 0xe1, 0x4d, 0x38, 0x39, 0x8f,
	0xe7, 0xd0, 0x60, 0x90, 0xb2, 0x0f, 0xbb, 0x70, 0x1f, 0xe6, 0x5b, 0x87, 0xb4, 0x43, 0xfd, 0xf5,
	0xf6, 0xf9, 0xad, 0xc0, 0x98, 0x17, 0x9f, 0x91, 0x15, 0x51, 0x55, 0x64, 0x6c, 0x18, 0x61, 0x0e,
	0xd9, 0x93, 0xab, 0xd6, 0x24, 0x2f, 0x6a, 0x3a, 0x22, 0xb1, 0x4f, 0xaa, 0x23, 0x48, 0xed, 0xb9,
	0x88, 0x8b, 0xa3, 0x6b, 0x26, 0x4c, 0xe8, 0x2d, 0x1c, 0x99, 0xbd, 0x5c, 0x58, 0x08, 0x50, 0xf2,
	0x2a, 0x62, 0xc1, 0x72, 0x66, 0x04, 0x10, 0x37, 0x6e, 0xfc, 0x44, 0xa9, 0xdf, 0xd4, 0x20, 0xdd,
	0xee, 0x41, 0xdb, 0x73, 0xde, 0x54, 0xec, 0xc9, 0xf3, 0x4b, 0x2e, 0xae, 0x5a, 0x4a, 0x5e, 0x47,
	0x07, 0x2b, 0x76, 0xa4, 0xe3, 0x28, 0xfe, 0xb0, 0xf0, 0x02, 0x06, 0xd1, 0xaf, 0x42, 0xc2, 0xa5,
	0xe0, 0x67, 0xbf, 0x16, 0x8e, 0x35, 0xce, 0x8a, 0xe5, 0x3d, 0x7b, 0x96, 0xd7, 0x79, 0x52, 0x1e,
	0xa1, 0xfb, 0x9b, 0xbe, 0x21, 0x9c, 0xe9, 0x56, 0x14, 0x7f, 0xa0, 0xe4, 0xc3, 0xc4, 0x46, 0xea,
	0xf7, 0xd2, 0x1d, 0x31, 0x0a, 0x5f, 0xeb, 0xa2, 0x68, 0x8d, 0xb5, 0xc5, 0x74, 0x0c, 0xdc, 0x82,
	0x80, 0x09, 0x19, 0x95, 0x71, 0x9a, 0x11, 0x57, 0x77, 0x4e, 0xc6, 0xff, 0x12, 0x03, 0xa7, 0xc7,
	0xf4, 0xc8, 0xb6, 0x7a, 0x59, 0x36, 0x3c, 0x53, 0xe2, 0x69, 0x8c, 0x25, 0x05, 0x45, 0x63, 0xf8,
	0x34, 0x89, 0x33, 0x3f, 0x85, 0x27, 0xbc, 0x65, 0xfa, 0xa8, 0x6d, 0x84, 0x5d, 0xba, 0x40, 0x32,
	0x30, 0xef, 0x83, 0x13, 0xa6, 0x78, 0xcc, 0x81, 0x9e, 0xda, 0xca, 0xd3, 0x7e, 0x9d, 0x6f, 0xcd,
	0xb7, 0xb3, 0xd8, 0xcf, 0x3b, 0x00, 0x92, 0xb8, 0x86, 0xac, 0x49, 0x7c, 0xf1, 0xd5, 0xcb, 0x7d,
};

const unsigned char kof99_address_0_7_xor[256] =
{
	0x74, 0xad, 0x5d, 0x1d, 0x9e, 0xc3, 0xfa, 0x4e, 0xf7, 0xdb, 0xca, 0xa2, 0x64, 0x36, 0x56, 0x0c,
	0x4f, 0xcf, 0x43, 0x66, 0x1e, 0x91, 0xe3, 0xa5, 0x58, 0xc2, 0xc1, 0xd4, 0xb9, 0xdd, 0x76, 0x16,
	0xce, 0x61, 0x75, 0x01, 0x2b, 0x22, 0x38, 0x55, 0x50, 0xef, 0x6c, 0x99, 0x05, 0xe9, 0xe8, 0xe0,
	0x2d, 0xa4, 0x4b, 0x4a, 0x42, 0xae, 0xba, 0x8c, 0x6f, 0x93, 0x14, 0xbd, 0x71, 0x21, 0xb0, 0x02,
	0x15, 0xc4, 0xe6, 0x60, 0xd7, 0x44, 0xfd, 0x85, 0x7e, 0x78, 0x8f, 0x00, 0x81, 0xf1, 0xa7, 0x3b,
	0xa0, 0x10, 0xf4, 0x9f, 0x39, 0x88, 0x35, 0x62, 0xcb, 0x19, 0x31, 0x11, 0x51, 0xfb, 0x2a, 0x20,
	0x45, 0xd3, 0x7d, 0x92, 0x1b, 0xf2, 0x09, 0x0d, 0x97, 0xa9, 0xb5, 0x3c, 0xee, 0x5c, 0xaf, 0x7b,
	0xd2, 0x3a, 0x49, 0x8e, 0xb6, 0xcd, 0xd9, 0xde, 0x8a, 0x29, 0x6e, 0xd8, 0x0b, 0xe1, 0x69, 0x87,
	0x1a, 0x96, 0x18, 0xcc, 0xdf, 0xe7, 0xc5, 0xc7, 0xf8, 0x52, 0xc9, 0xf0, 0xb7, 0xe5, 0x33, 0xda,
	0x67, 0x9d, 0xa3, 0x03, 0x0e, 0x72, 0x26, 0x79, 0xe2, 0xb8, 0xfc, 0xaa, 0xfe, 0xb4, 0x86, 0xc8,
	0xd1, 0xbc, 0x12, 0x08, 0x77, 0xeb, 0x40, 0x8d, 0x04, 0x25, 0x4d, 0x5a, 0x6a, 0x7a, 0x2e, 0x41,
	0x65, 0x1c, 0x13, 0x94, 0xb2, 0x63, 0x28, 0x59, 0x5e, 0x9a, 0x30, 0x07, 0xc6, 0xbf, 0x17, 0xf5,
	0x0f, 0x89, 0xf3, 0x1f, 0xea, 0x6d, 0xb3, 0xc0, 0x70, 0x47, 0xf9, 0x53, 0xf6, 0xd6, 0x54, 0xed,
	0x6b, 0x4c, 0xe4, 0x8b, 0x83, 0x24, 0x90, 0xb1, 0x7c, 0xbb, 0x73, 0xab, 0xd5, 0x2f, 0x5f, 0xec,
	0x9c, 0x2c, 0xa8, 0x34, 0x46, 0x37, 0x27, 0xa1, 0x0a, 0x06, 0x80, 0x68, 0x82, 0x32, 0x84, 0xff,
	0x48, 0xac, 0x7f, 0x3f, 0x95, 0xdc, 0x98, 0x9b, 0xbe, 0x23, 0x57, 0x3e, 0x5b, 0xd0, 0x3d, 0xa6,
};






const unsigned char kof2000_type0_t03[256] =
{
	0x10, 0x61, 0xf1, 0x78, 0x85, 0x52, 0x68, 0xe3, 0x12, 0x0d, 0xfa, 0xf0, 0xc9, 0x36, 0x5e, 0x3d,
	0xf9, 0xa6, 0x01, 0x2e, 0xc7, 0x84, 0xea, 0x2b, 0x6d, 0x14, 0x38, 0x4f, 0x55, 0x1c, 0x9d, 0xa7,
	0x7a, 0xc6, 0xf8, 0x9a, 0xe6, 0x42, 0xb5, 0xed, 0x7d, 0x3a, 0xb1, 0x05, 0x43, 0x4a, 0x22, 0xfd,
	0xac, 0xa4, 0x31, 0xc3, 0x32, 0x76, 0x95, 0x9e, 0x7e, 0x88, 0x8e, 0xa2, 0x97, 0x18, 0xbe, 0x2a,
	0xf5, 0xd6, 0xca, 0xcc, 0x72, 0x3b, 0x87, 0x6c, 0xde, 0x75, 0xd7, 0x21, 0xcb, 0x0b, 0xdd, 0xe7,
	0xe1, 0x65, 0xaa, 0xb9, 0x44, 0xfb, 0x66, 0x15, 0x1a, 0x3c, 0x98, 0xcf, 0x8a, 0xdf, 0x37, 0xa5,
	0x2f, 0x67, 0xd2, 0x83, 0xb6, 0x6b, 0xfc, 0xe0, 0xb4, 0x7c, 0x08, 0xdc, 0x93, 0x30, 0xab, 0xe4,
	0x19, 0xc2, 0x8b, 0xeb, 0xa0, 0x0a, 0xc8, 0x03, 0xc0, 0x4b, 0x64, 0x71, 0x86, 0x9c, 0x9b, 0x16,
	0x79, 0xff, 0x70, 0x09, 0x8c, 0xd0, 0xf6, 0x53, 0x07, 0x73, 0xd4, 0x89, 0xb3, 0x00, 0xe9, 0xfe,
	0xec, 0x8f, 0xbc, 0xb2, 0x1e, 0x5d, 0x11, 0x35, 0xa9, 0x06, 0x59, 0x9f, 0xc1, 0xd3, 0x7b, 0xf2,
	0xc5, 0x77, 0x4e, 0x39, 0x20, 0xd5, 0x6a, 0x82, 0xda, 0x45, 0xf3, 0x33, 0x81, 0x23, 0xba, 0xe2,
	0x1d, 0x5f, 0x5c, 0x51, 0x49, 0xae, 0x8d, 0xc4, 0xa8, 0xf7, 0x1f, 0x0f, 0x34, 0x28, 0xa1, 0xd9,
	0x27, 0xd8, 0x4c, 0x2c, 0xbf, 0x91, 0x3e, 0x69, 0x57, 0x41, 0x25, 0x0c, 0x5a, 0x90, 0x92, 0xb0,
	0x63, 0x6f, 0x40, 0xaf, 0x74, 0xb8, 0x2d, 0x80, 0xbb, 0x46, 0x94, 0xe5, 0x29, 0xee, 0xb7, 0x1b,
	0x96, 0xad, 0x13, 0x0e, 0x58, 0x99, 0x60, 0x4d, 0x17, 0x26, 0xce, 0xe8, 0xdb, 0xef, 0x24, 0xa3,
	0x6e, 0x7f, 0x54, 0x3f, 0x02, 0xd1, 0x5b, 0x50, 0x56, 0x48, 0xf4, 0xbd, 0x62, 0x47, 0x04, 0xcd,
};

const unsigned char kof2000_type0_t12[256] =
{
	0xf4, 0x28, 0xb4, 0x8f, 0xfa, 0xeb, 0x8e, 0x54, 0x2b, 0x49, 0xd1, 0x76, 0x71, 0x47, 0x8b, 0x57,
	0x92, 0x85, 0x7c, 0xb8, 0x5c, 0x22, 0xf9, 0x26, 0xbc, 0x5b, 0x6d, 0x67, 0xae, 0x5f, 0x6f, 0xf5,
	0x9f, 0x48, 0x66, 0x40, 0x0d, 0x11, 0x4e, 0xb2, 0x6b, 0x35, 0x15, 0x0f, 0x18, 0x25, 0x1d, 0xba,
	0xd3, 0x69, 0x79, 0xec, 0xa8, 0x8c, 0xc9, 0x7f, 0x4b, 0xdb, 0x51, 0xaf, 0xca, 0xe2, 0xb3, 0x81,
	0x12, 0x5e, 0x7e, 0x38, 0xc8, 0x95, 0x01, 0xff, 0xfd, 0xfb, 0xf2, 0x74, 0x62, 0x14, 0xa5, 0x98,
	0xa6, 0xda, 0x80, 0x53, 0xe8, 0x56, 0xac, 0x1b, 0x52, 0xd0, 0xf1, 0x45, 0x42, 0xb6, 0x1a, 0x4a,
	0x3a, 0x99, 0xfc, 0xd2, 0x9c, 0xcf, 0x31, 0x2d, 0xdd, 0x86, 0x2f, 0x29, 0xe1, 0x03, 0x19, 0xa2,
	0x41, 0x33, 0x83, 0x90, 0xc1, 0xbf, 0x0b, 0x08, 0x3d, 0xd8, 0x8d, 0x6c, 0x39, 0xa0, 0xe3, 0x55,
	0x02, 0x50, 0x46, 0xe6, 0xc3, 0x82, 0x36, 0x13, 0x75, 0xab, 0x27, 0xd7, 0x1f, 0x0a, 0xd4, 0x89,
	0x59, 0x4f, 0xc0, 0x5d, 0xc6, 0xf7, 0x88, 0xbd, 0x3c, 0x00, 0xef, 0xcd, 0x05, 0x1c, 0xaa, 0x9b,
	0xed, 0x7a, 0x61, 0x17, 0x93, 0xfe, 0x23, 0xb9, 0xf3, 0x68, 0x78, 0xf6, 0x5a, 0x7b, 0xe0, 0xe4,
	0xa3, 0xee, 0x16, 0x72, 0xc7, 0x3b, 0x8a, 0x37, 0x2a, 0x70, 0xa9, 0x2c, 0x21, 0xf8, 0x24, 0x09,
	0xce, 0x20, 0x9e, 0x06, 0x87, 0xc5, 0x04, 0x64, 0x43, 0x7d, 0x4d, 0x10, 0xd6, 0xa4, 0x94, 0x4c,
	0x60, 0xde, 0xdf, 0x58, 0xb1, 0x44, 0x3f, 0xb0, 0xd9, 0xe5, 0xcb, 0xbb, 0xbe, 0xea, 0x07, 0x34,
	0x73, 0x6a, 0x77, 0xf0, 0x9d, 0x0c, 0x2e, 0x0e, 0x91, 0x9a, 0xcc, 0xc2, 0xb7, 0x63, 0x97, 0xd5,
	0xdc, 0xc4, 0x32, 0xe7, 0x84, 0x3e, 0x30, 0xa1, 0x1e, 0xb5, 0x6e, 0x65, 0xe9, 0xad, 0xa7, 0x96,
};

const unsigned char kof2000_type1_t03[256] =
{
	0x9a, 0x2f, 0xcc, 0x4e, 0x40, 0x69, 0xac, 0xca, 0xa5, 0x7b, 0x0a, 0x61, 0x91, 0x0d, 0x55, 0x74,
	0xcd, 0x8b, 0x0b, 0x80, 0x09, 0x5e, 0x38, 0xc7, 0xda, 0xbf, 0xf5, 0x37, 0x23, 0x31, 0x33, 0xe9,
	0xae, 0x87, 0xe5, 0xfa, 0x6e, 0x5c, 0xad, 0xf4, 0x76, 0x62, 0x9f, 0x2e, 0x01, 0xe2, 0xf6, 0x47,
	0x8c, 0x7c, 0xaa, 0x98, 0xb5, 0x92, 0x51, 0xec, 0x5f, 0x07, 0x5d, 0x6f, 0x16, 0xa1, 0x1d, 0xa9,
	0x48, 0x45, 0xf0, 0x6a, 0x9c, 0x1e, 0x11, 0xa0, 0x06, 0x46, 0xd5, 0xf1, 0x73, 0xed, 0x94, 0xf7,
	0xc3, 0x57, 0x1b, 0xe0, 0x97, 0xb1, 0xa4, 0xa7, 0x24, 0xe7, 0x2b, 0x05, 0x5b, 0x34, 0x0c, 0xb8,
	0x0f, 0x9b, 0xc8, 0x4d, 0x5a, 0xa6, 0x86, 0x3e, 0x14, 0x29, 0x84, 0x58, 0x90, 0xdb, 0x2d, 0x54,
	0x9d, 0x82, 0xd4, 0x7d, 0xc6, 0x67, 0x41, 0x89, 0xc1, 0x13, 0xb0, 0x9e, 0x81, 0x6d, 0xa8, 0x59,
	0xbd, 0x39, 0x8e, 0xe6, 0x25, 0x8f, 0xd9, 0xa2, 0xe4, 0x53, 0xc5, 0x72, 0x7e, 0x36, 0x4a, 0x4f,
	0x52, 0xc2, 0x22, 0x2a, 0xce, 0x3c, 0x21, 0x2c, 0x00, 0xd7, 0x75, 0x8a, 0x27, 0xee, 0x43, 0xfe,
	0xcb, 0x6b, 0xb9, 0xa3, 0x78, 0xb7, 0x85, 0x02, 0x20, 0xd0, 0x83, 0xc4, 0x12, 0xf9, 0xfd, 0xd8,
	0x79, 0x64, 0x3a, 0x49, 0x03, 0xb4, 0xc0, 0xf2, 0xdf, 0x15, 0x93, 0x08, 0x35, 0xff, 0x70, 0xdd,
	0x28, 0x6c, 0x0e, 0x04, 0xde, 0x7a, 0x65, 0xd2, 0xab, 0x42, 0x95, 0xe1, 0x3f, 0x3b, 0x7f, 0x66,
	0xd1, 0x8d, 0xe3, 0xbb, 0x1c, 0xfc, 0x77, 0x1a, 0x88, 0x18, 0x19, 0x68, 0x1f, 0x56, 0xd6, 0xe8,
	0xb6, 0xbc, 0xd3, 0xea, 0x3d, 0x26, 0xb3, 0xc9, 0x44, 0xdc, 0xf3, 0x32, 0x30, 0xef, 0x96, 0x4c,
	0xaf, 0x17, 0xf8, 0xfb, 0x60, 0x50, 0xeb, 0x4b, 0x99, 0x63, 0xba, 0xb2, 0x71, 0xcf, 0x10, 0xbe,
};

const unsigned char kof2000_type1_t12[256] =
{
	0xda, 0xa7, 0xd6, 0x6e, 0x2f, 0x5e, 0xf0, 0x3f, 0xa4, 0xce, 0xd3, 0xfd, 0x46, 0x2a, 0xac, 0xc9,
	0xbe, 0xeb, 0x9f, 0xd5, 0x3c, 0x61, 0x96, 0x11, 0xd0, 0x38, 0xca, 0x06, 0xed, 0x1b, 0x65, 0xe7,
	0x23, 0xdd, 0xd9, 0x05, 0xbf, 0x5b, 0x5d, 0xa5, 0x95, 0x00, 0xec, 0xf1, 0x01, 0xa9, 0xa6, 0xfc,
	0xbb, 0x54, 0xe3, 0x2e, 0x92, 0x58, 0x0a, 0x7b, 0xb6, 0xcc, 0xb1, 0x5f, 0x14, 0x35, 0x72, 0xff,
	0xe6, 0x52, 0xd7, 0x8c, 0xf3, 0x43, 0xaf, 0x9c, 0xc0, 0x4f, 0x0c, 0x42, 0x8e, 0xef, 0x80, 0xcd,
	0x1d, 0x7e, 0x88, 0x3b, 0x98, 0xa1, 0xad, 0xe4, 0x9d, 0x8d, 0x2b, 0x56, 0xb5, 0x50, 0xdf, 0x66,
	0x6d, 0xd4, 0x60, 0x09, 0xe1, 0xee, 0x4a, 0x47, 0xf9, 0xfe, 0x73, 0x07, 0x89, 0xa8, 0x39, 0xea,
	0x82, 0x9e, 0xcf, 0x26, 0xb2, 0x4e, 0xc3, 0x59, 0xf2, 0x3d, 0x9a, 0xb0, 0x69, 0xf7, 0xbc, 0x34,
	0xe5, 0x36, 0x22, 0xfb, 0x57, 0x71, 0x99, 0x6c, 0x83, 0x30, 0x55, 0xc2, 0xbd, 0xf4, 0x77, 0xe9,
	0x76, 0x97, 0xa0, 0xe0, 0xb9, 0x86, 0x6b, 0xa3, 0x84, 0x67, 0x1a, 0x70, 0x02, 0x5a, 0x41, 0x5c,
	0x25, 0x81, 0xaa, 0x28, 0x78, 0x4b, 0xc6, 0x64, 0x53, 0x16, 0x4d, 0x8b, 0x20, 0x93, 0xae, 0x0f,
	0x94, 0x2c, 0x3a, 0xc7, 0x62, 0xe8, 0xc4, 0xdb, 0x04, 0xc5, 0xfa, 0x29, 0x48, 0xd1, 0x08, 0x24,
	0x0d, 0xe2, 0xd8, 0x10, 0xb4, 0x91, 0x8a, 0x13, 0x0e, 0xdc, 0xd2, 0x79, 0xb8, 0xf8, 0xba, 0x2d,
	0xcb, 0xf5, 0x7d, 0x37, 0x51, 0x40, 0x31, 0xa2, 0x0b, 0x18, 0x63, 0x7f, 0xb3, 0xab, 0x9b, 0x87,
	0xf6, 0x90, 0xde, 0xc8, 0x27, 0x45, 0x7c, 0x1c, 0x85, 0x68, 0x33, 0x19, 0x03, 0x75, 0x15, 0x7a,
	0x1f, 0x49, 0x8f, 0x4c, 0xc1, 0x44, 0x17, 0x12, 0x6f, 0x32, 0xb7, 0x3e, 0x74, 0x1e, 0x21, 0x6a,
};



const unsigned char kof2000_address_8_15_xor1[256] =
{
	0xfc, 0x9b, 0x1c, 0x35, 0x72, 0x53, 0xd6, 0x7d, 0x84, 0xa4, 0xc5, 0x93, 0x7b, 0xe7, 0x47, 0xd5,
	0x24, 0xa2, 0xfa, 0x19, 0x0c, 0xb1, 0x8c, 0xb9, 0x9d, 0xd8, 0x59, 0x4f, 0x3c, 0xb2, 0x78, 0x4a,
	0x2a, 0x96, 0x9a, 0xf1, 0x1f, 0x22, 0xa8, 0x5b, 0x67, 0xa3, 0x0f, 0x00, 0xfb, 0xdf, 0xeb, 0x0a,
	0x57, 0xb8, 0x25, 0xd7, 0xf0, 0x6b, 0x0b, 0x31, 0x95, 0x23, 0x2d, 0x5c, 0x27, 0xc7, 0xf4, 0x55,
	0x1a, 0xf7, 0x74, 0xbe, 0xd3, 0xac, 0x3d, 0xc1, 0x7f, 0xbd, 0x28, 0x01, 0x10, 0xe5, 0x09, 0x37,
	0x1e, 0x58, 0xaf, 0x17, 0xf2, 0x16, 0x30, 0x92, 0x36, 0x68, 0xe6, 0xd4, 0xea, 0xb7, 0x75, 0x54,
	0x77, 0x41, 0xb4, 0x8d, 0xe0, 0xf3, 0x51, 0x03, 0xa9, 0xe8, 0x66, 0xab, 0x29, 0xa5, 0xed, 0xcb,
	0xd1, 0xaa, 0xf5, 0xdb, 0x4c, 0x42, 0x97, 0x8a, 0xae, 0xc9, 0x6e, 0x04, 0x33, 0x85, 0xdd, 0x2b,
	0x6f, 0xef, 0x12, 0x21, 0x7a, 0xa1, 0x5a, 0x91, 0xc8, 0xcc, 0xc0, 0xa7, 0x60, 0x3e, 0x56, 0x2f,
	0xe4, 0x71, 0x99, 0xc2, 0xa0, 0x45, 0x80, 0x65, 0xbb, 0x87, 0x69, 0x81, 0x73, 0xca, 0xf6, 0x46,
	0x43, 0xda, 0x26, 0x7e, 0x8f, 0xe1, 0x8b, 0xfd, 0x50, 0x79, 0xba, 0xc6, 0x63, 0x4b, 0xb3, 0x8e,
	0x34, 0xe2, 0x48, 0x14, 0xcd, 0xe3, 0xc4, 0x05, 0x13, 0x40, 0x06, 0x6c, 0x88, 0xb0, 0xe9, 0x1b,
	0x4d, 0xf8, 0x76, 0x02, 0x44, 0x94, 0xcf, 0x32, 0xfe, 0xce, 0x3b, 0x5d, 0x2c, 0x89, 0x5f, 0xdc,
	0xd2, 0x9c, 0x6a, 0xec, 0x18, 0x6d, 0x0e, 0x86, 0xff, 0x5e, 0x9e, 0xee, 0x11, 0xd0, 0x49, 0x52,
	0x4e, 0x61, 0x90, 0x0d, 0xc3, 0x39, 0x15, 0x83, 0xb5, 0x62, 0x3f, 0x70, 0x7c, 0xad, 0x20, 0xbf,
	0x2e, 0x08, 0x1d, 0xf9, 0xb6, 0xa6, 0x64, 0x07, 0x82, 0x38, 0x98, 0x3a, 0x9f, 0xde, 0xbc, 0xd9,
};

const unsigned char kof2000_address_8_15_xor2[256] =
{
	0x00, 0xbe, 0x06, 0x5a, 0xfa, 0x42, 0x15, 0xf2, 0x3f, 0x0a, 0x84, 0x93, 0x4e, 0x78, 0x3b, 0x89,
	0x32, 0x98, 0xa2, 0x87, 0x73, 0xdd, 0x26, 0xe5, 0x05, 0x71, 0x08, 0x6e, 0x9b, 0xe0, 0xdf, 0x9e,
	0xfc, 0x83, 0x81, 0xef, 0xb2, 0xc0, 0xc3, 0xbf, 0xa7, 0x6d, 0x1b, 0x95, 0xed, 0xb9, 0x3e, 0x13,
	0xb0, 0x47, 0x9c, 0x7a, 0x24, 0x41, 0x68, 0xd0, 0x36, 0x0b, 0xb5, 0xc2, 0x67, 0xf7, 0x54, 0x92,
	0x1e, 0x44, 0x86, 0x2b, 0x94, 0xcc, 0xba, 0x23, 0x0d, 0xca, 0x6b, 0x4c, 0x2a, 0x9a, 0x2d, 0x8b,
	0xe3, 0x52, 0x29, 0xf0, 0x21, 0xbd, 0xbb, 0x1f, 0xa3, 0xab, 0xf8, 0x46, 0xb7, 0x45, 0x82, 0x5e,
	0xdb, 0x07, 0x5d, 0xe9, 0x9d, 0x1a, 0x48, 0xce, 0x91, 0x12, 0xd4, 0xee, 0xa9, 0x39, 0xf1, 0x18,
	0x2c, 0x22, 0x8a, 0x7e, 0x34, 0x4a, 0x8c, 0xc1, 0x14, 0xf3, 0x20, 0x35, 0xd9, 0x96, 0x33, 0x77,
	0x9f, 0x76, 0x7c, 0x90, 0xc6, 0xd5, 0xa1, 0x5b, 0xac, 0x75, 0xc7, 0x0c, 0xb3, 0x17, 0xd6, 0x99,
	0x56, 0xa6, 0x3d, 0x1d, 0xb1, 0x2e, 0xd8, 0xbc, 0x2f, 0xde, 0x60, 0x55, 0x6c, 0x40, 0xcd, 0x43,
	0xff, 0xad, 0x38, 0x79, 0x51, 0xc8, 0x0e, 0x5f, 0xc4, 0x66, 0xcb, 0xa8, 0x7d, 0xa4, 0x3a, 0xea,
	0x27, 0x7b, 0x70, 0x8e, 0x5c, 0x19, 0x0f, 0x80, 0x6f, 0x8f, 0x10, 0xf9, 0x49, 0x85, 0x69, 0x7f,
	0xeb, 0x1c, 0x01, 0x65, 0x37, 0xa5, 0x28, 0xe4, 0x6a, 0x03, 0x04, 0xd1, 0x31, 0x11, 0x30, 0xfb,
	0x88, 0x97, 0xd3, 0xf6, 0xc5, 0x4d, 0xf5, 0x3c, 0xe8, 0x61, 0xdc, 0xd2, 0xb4, 0xb8, 0xa0, 0xae,
	0x16, 0x25, 0x02, 0x09, 0xfe, 0xcf, 0x53, 0x63, 0xaf, 0x59, 0xf4, 0xe1, 0xec, 0xd7, 0xe7, 0x50,
	0xe2, 0xc9, 0xaa, 0x4b, 0x8d, 0x4f, 0xe6, 0x64, 0xda, 0x74, 0xb6, 0x72, 0x57, 0x62, 0xfd, 0x58,
};

const unsigned char kof2000_address_16_23_xor1[256] =
{
	0x45, 0x9f, 0x6e, 0x2f, 0x28, 0xbc, 0x5e, 0x6d, 0xda, 0xb5, 0x0d, 0xb8, 0xc0, 0x8e, 0xa2, 0x32,
	0xee, 0xcd, 0x8d, 0x48, 0x8c, 0x27, 0x14, 0xeb, 0x65, 0xd7, 0xf2, 0x93, 0x99, 0x90, 0x91, 0xfc,
	0x5f, 0xcb, 0xfa, 0x75, 0x3f, 0x26, 0xde, 0x72, 0x33, 0x39, 0xc7, 0x1f, 0x88, 0x79, 0x73, 0xab,
	0x4e, 0x36, 0x5d, 0x44, 0xd2, 0x41, 0xa0, 0x7e, 0xa7, 0x8b, 0xa6, 0xbf, 0x03, 0xd8, 0x86, 0xdc,
	0x2c, 0xaa, 0x70, 0x3d, 0x46, 0x07, 0x80, 0x58, 0x0b, 0x2b, 0xe2, 0xf0, 0xb1, 0xfe, 0x42, 0xf3,
	0xe9, 0xa3, 0x85, 0x78, 0xc3, 0xd0, 0x5a, 0xdb, 0x1a, 0xfb, 0x9d, 0x8a, 0xa5, 0x12, 0x0e, 0x54,
	0x8f, 0xc5, 0x6c, 0xae, 0x25, 0x5b, 0x4b, 0x17, 0x02, 0x9c, 0x4a, 0x24, 0x40, 0xe5, 0x9e, 0x22,
	0xc6, 0x49, 0x62, 0xb6, 0x6b, 0xbb, 0xa8, 0xcc, 0xe8, 0x81, 0x50, 0x47, 0xc8, 0xbe, 0x5c, 0xa4,
	0xd6, 0x94, 0x4f, 0x7b, 0x9a, 0xcf, 0xe4, 0x59, 0x7a, 0xa1, 0xea, 0x31, 0x37, 0x13, 0x2d, 0xaf,
	0x21, 0x69, 0x19, 0x1d, 0x6f, 0x16, 0x98, 0x1e, 0x08, 0xe3, 0xb2, 0x4d, 0x9b, 0x7f, 0xa9, 0x77,
	0xed, 0xbd, 0xd4, 0xd9, 0x34, 0xd3, 0xca, 0x09, 0x18, 0x60, 0xc9, 0x6a, 0x01, 0xf4, 0xf6, 0x64,
	0xb4, 0x3a, 0x15, 0xac, 0x89, 0x52, 0x68, 0x71, 0xe7, 0x82, 0xc1, 0x0c, 0x92, 0xf7, 0x30, 0xe6,
	0x1c, 0x3e, 0x0f, 0x0a, 0x67, 0x35, 0xba, 0x61, 0xdd, 0x29, 0xc2, 0xf8, 0x97, 0x95, 0xb7, 0x3b,
	0xe0, 0xce, 0xf9, 0xd5, 0x06, 0x76, 0xb3, 0x05, 0x4c, 0x04, 0x84, 0x3c, 0x87, 0x23, 0x63, 0x7c,
	0x53, 0x56, 0xe1, 0x7d, 0x96, 0x1b, 0xd1, 0xec, 0x2a, 0x66, 0xf1, 0x11, 0x10, 0xff, 0x43, 0x2e,
	0xdf, 0x83, 0x74, 0xf5, 0x38, 0x20, 0xfd, 0xad, 0xc4, 0xb9, 0x55, 0x51, 0xb0, 0xef, 0x00, 0x57,
};

const unsigned char kof2000_address_16_23_xor2[256] =
{
	0x00, 0xb8, 0xf0, 0x34, 0xca, 0x21, 0x3c, 0xf9, 0x01, 0x8e, 0x75, 0x70, 0xec, 0x13, 0x27, 0x96,
	0xf4, 0x5b, 0x88, 0x1f, 0xeb, 0x4a, 0x7d, 0x9d, 0xbe, 0x02, 0x14, 0xaf, 0xa2, 0x06, 0xc6, 0xdb,
	0x35, 0x6b, 0x74, 0x45, 0x7b, 0x29, 0xd2, 0xfe, 0xb6, 0x15, 0xd0, 0x8a, 0xa9, 0x2d, 0x19, 0xf6,
	0x5e, 0x5a, 0x90, 0xe9, 0x11, 0x33, 0xc2, 0x47, 0x37, 0x4c, 0x4f, 0x59, 0xc3, 0x04, 0x57, 0x1d,
	0xf2, 0x63, 0x6d, 0x6e, 0x31, 0x95, 0xcb, 0x3e, 0x67, 0xb2, 0xe3, 0x98, 0xed, 0x8d, 0xe6, 0xfb,
	0xf8, 0xba, 0x5d, 0xd4, 0x2a, 0xf5, 0x3b, 0x82, 0x05, 0x16, 0x44, 0xef, 0x4d, 0xe7, 0x93, 0xda,
	0x9f, 0xbb, 0x61, 0xc9, 0x53, 0xbd, 0x76, 0x78, 0x52, 0x36, 0x0c, 0x66, 0xc1, 0x10, 0xdd, 0x7a,
	0x84, 0x69, 0xcd, 0xfd, 0x58, 0x0d, 0x6c, 0x89, 0x68, 0xad, 0x3a, 0xb0, 0x4b, 0x46, 0xc5, 0x03,
	0xb4, 0xf7, 0x30, 0x8c, 0x4e, 0x60, 0x73, 0xa1, 0x8b, 0xb1, 0x62, 0xcc, 0xd1, 0x08, 0xfc, 0x77,
	0x7e, 0xcf, 0x56, 0x51, 0x07, 0xa6, 0x80, 0x92, 0xdc, 0x0b, 0xa4, 0xc7, 0xe8, 0xe1, 0xb5, 0x71,
	0xea, 0xb3, 0x2f, 0x94, 0x18, 0xe2, 0x3d, 0x49, 0x65, 0xaa, 0xf1, 0x91, 0xc8, 0x99, 0x55, 0x79,
	0x86, 0xa7, 0x26, 0xa0, 0xac, 0x5f, 0xce, 0x6a, 0x5c, 0xf3, 0x87, 0x8f, 0x12, 0x1c, 0xd8, 0xe4,
	0x9b, 0x64, 0x2e, 0x1e, 0xd7, 0xc0, 0x17, 0xbc, 0xa3, 0xa8, 0x9a, 0x0e, 0x25, 0x40, 0x41, 0x50,
	0xb9, 0xbf, 0x28, 0xdf, 0x32, 0x54, 0x9e, 0x48, 0xd5, 0x2b, 0x42, 0xfa, 0x9c, 0x7f, 0xd3, 0x85,
	0x43, 0xde, 0x81, 0x0f, 0x24, 0xc4, 0x38, 0xae, 0x83, 0x1b, 0x6f, 0x7c, 0xe5, 0xff, 0x1a, 0xd9,
	0x3f, 0xb7, 0x22, 0x97, 0x09, 0xe0, 0xa5, 0x20, 0x23, 0x2c, 0x72, 0xd6, 0x39, 0xab, 0x0a, 0xee,
};

const unsigned char kof2000_address_0_7_xor[256] =
{
	0x26, 0x48, 0x06, 0x9b, 0x21, 0xa9, 0x1b, 0x76, 0xc9, 0xf8, 0xb4, 0x67, 0xe4, 0xff, 0x99, 0xf7,
	0x15, 0x9e, 0x62, 0x00, 0x72, 0x4d, 0xa0, 0x4f, 0x02, 0xf1, 0xea, 0xef, 0x0b, 0xf3, 0xeb, 0xa6,
	0x93, 0x78, 0x6f, 0x7c, 0xda, 0xd4, 0x7b, 0x05, 0xe9, 0xc6, 0xd6, 0xdb, 0x50, 0xce, 0xd2, 0x01,
	0xb5, 0xe8, 0xe0, 0x2a, 0x08, 0x1a, 0xb8, 0xe3, 0xf9, 0xb1, 0xf4, 0x8b, 0x39, 0x2d, 0x85, 0x9c,
	0x55, 0x73, 0x63, 0x40, 0x38, 0x96, 0xdc, 0xa3, 0xa2, 0xa1, 0x25, 0x66, 0x6d, 0x56, 0x8e, 0x10,
	0x0f, 0x31, 0x1c, 0xf5, 0x28, 0x77, 0x0a, 0xd1, 0x75, 0x34, 0xa4, 0xfe, 0x7d, 0x07, 0x51, 0x79,
	0x41, 0x90, 0x22, 0x35, 0x12, 0xbb, 0xc4, 0xca, 0xb2, 0x1f, 0xcb, 0xc8, 0xac, 0xdd, 0xd0, 0x0d,
	0xfc, 0xc5, 0x9d, 0x14, 0xbc, 0x83, 0xd9, 0x58, 0xc2, 0x30, 0x9a, 0x6a, 0xc0, 0x0c, 0xad, 0xf6,
	0x5d, 0x74, 0x7f, 0x2f, 0xbd, 0x1d, 0x47, 0xd5, 0xe6, 0x89, 0xcf, 0xb7, 0xd3, 0x59, 0x36, 0x98,
	0xf0, 0xfb, 0x3c, 0xf2, 0x3f, 0xa7, 0x18, 0x82, 0x42, 0x5c, 0xab, 0xba, 0xde, 0x52, 0x09, 0x91,
	0xaa, 0x61, 0xec, 0xd7, 0x95, 0x23, 0xcd, 0x80, 0xa5, 0x68, 0x60, 0x27, 0x71, 0xe1, 0x2c, 0x2e,
	0x8d, 0x2b, 0x57, 0x65, 0xbf, 0xc1, 0x19, 0xc7, 0x49, 0x64, 0x88, 0x4a, 0xcc, 0x20, 0x4e, 0xd8,
	0x3b, 0x4c, 0x13, 0x5f, 0x9f, 0xbe, 0x5e, 0x6e, 0xfd, 0xe2, 0xfa, 0x54, 0x37, 0x0e, 0x16, 0x7a,
	0x6c, 0x33, 0xb3, 0x70, 0x84, 0x7e, 0xc3, 0x04, 0xb0, 0xae, 0xb9, 0x81, 0x03, 0x29, 0xdf, 0x46,
	0xe5, 0x69, 0xe7, 0x24, 0x92, 0x5a, 0x4b, 0x5b, 0x94, 0x11, 0x3a, 0x3d, 0x87, 0xed, 0x97, 0xb6,
	0x32, 0x3e, 0x45, 0xaf, 0x1e, 0x43, 0x44, 0x8c, 0x53, 0x86, 0x6b, 0xee, 0xa8, 0x8a, 0x8f, 0x17,
};




static void decrypt(unsigned char *r0, unsigned char *r1,
		    unsigned char c0,  unsigned char c1,
		    const unsigned char *table0hi,
		    const unsigned char *table0lo,
		    const unsigned char *table1,
		    int base,
		    int invert)

{
	unsigned char tmp,xor0,xor1;

	tmp = table1[(base & 0xff) ^ address_0_7_xor[(base >> 8) & 0xff]];
	xor0 = (table0hi[(base >> 8) & 0xff] & 0xfe) | (tmp & 0x01);
	xor1 = (tmp & 0xfe) | (table0lo[(base >> 8) & 0xff] & 0x01);

	if (invert)
	{
		*r0 = c1 ^ xor0;
		*r1 = c0 ^ xor1;
	}
	else
	{
		*r0 = c0 ^ xor0;
		*r1 = c1 ^ xor1;
	}
}

static void neogeo_gfx_decrypt(int extra_xor)
{
	int rom_size;
	UINT8 *buf;
	UINT8 *rom;
	int rpos;

	rom_size = memory_region_length(REGION_GFX3);

	buf = malloc(rom_size);

	if (!buf) return;

	rom = memory_region(REGION_GFX3);

	/* Data xor*/
	for (rpos = 0;rpos < rom_size/4;rpos++)
	{
		decrypt(buf+4*rpos+0, buf+4*rpos+3, rom[4*rpos+0], rom[4*rpos+3], type0_t03, type0_t12, type1_t03, rpos, (rpos>>8) & 1);
		decrypt(buf+4*rpos+1, buf+4*rpos+2, rom[4*rpos+1], rom[4*rpos+2], type0_t12, type0_t03, type1_t12, rpos, ((rpos>>16) ^ address_16_23_xor2[(rpos>>8) & 0xff]) & 1);
	}

	/* Address xor*/
	for (rpos = 0;rpos < rom_size/4;rpos++)
	{
		int baser;

		baser = rpos;

		baser ^= extra_xor;

		baser ^= address_8_15_xor1[(baser >> 16) & 0xff] << 8;
		baser ^= address_8_15_xor2[baser & 0xff] << 8;
		baser ^= address_16_23_xor1[baser & 0xff] << 16;
		baser ^= address_16_23_xor2[(baser >> 8) & 0xff] << 16;
		baser ^= address_0_7_xor[(baser >> 8) & 0xff];

		/* special handling for preisle2 */
		if (rom_size == 0x3000000)
		{
			if (rpos < 0x2000000/4)
				baser &= (0x2000000/4)-1;
			else
				baser = 0x2000000/4 + (baser & ((0x1000000/4)-1));
		}
		else	/* Clamp to the real rom size */
			baser &= (rom_size/4)-1;

		rom[4*rpos+0] = buf[4*baser+0];
		rom[4*rpos+1] = buf[4*baser+1];
		rom[4*rpos+2] = buf[4*baser+2];
		rom[4*rpos+3] = buf[4*baser+3];
	}

	free(buf);


	/* the S data comes from the end fo the C data */
	{
		int i;
		int tx_size = memory_region_length(REGION_GFX1);
		UINT8 *src = memory_region(REGION_GFX3)+rom_size-tx_size;
		UINT8 *dst = memory_region(REGION_GFX1);

		for (i = 0;i < tx_size;i++)
			dst[i] = src[(i & ~0x1f) + ((i & 7) << 2) + ((~i & 8) >> 2) + ((i & 0x10) >> 4)];
	}
}


/* CMC42 protection chip */
void kof99_neogeo_gfx_decrypt(int extra_xor)
{
	type0_t03 =          kof99_type0_t03;
	type0_t12 =          kof99_type0_t12;
	type1_t03 =          kof99_type1_t03;
	type1_t12 =          kof99_type1_t12;
	address_8_15_xor1 =  kof99_address_8_15_xor1;
	address_8_15_xor2 =  kof99_address_8_15_xor2;
	address_16_23_xor1 = kof99_address_16_23_xor1;
	address_16_23_xor2 = kof99_address_16_23_xor2;
	address_0_7_xor =    kof99_address_0_7_xor;
	neogeo_gfx_decrypt(extra_xor);
}

/* CMC50 protection chip */
void kof2000_neogeo_gfx_decrypt(int extra_xor)
{
	type0_t03 =          kof2000_type0_t03;
	type0_t12 =          kof2000_type0_t12;
	type1_t03 =          kof2000_type1_t03;
	type1_t12 =          kof2000_type1_t12;
	address_8_15_xor1 =  kof2000_address_8_15_xor1;
	address_8_15_xor2 =  kof2000_address_8_15_xor2;
	address_16_23_xor1 = kof2000_address_16_23_xor1;
	address_16_23_xor2 = kof2000_address_16_23_xor2;
	address_0_7_xor =    kof2000_address_0_7_xor;
	neogeo_gfx_decrypt(extra_xor);

	/* here I should also decrypt the sound ROM */
}
