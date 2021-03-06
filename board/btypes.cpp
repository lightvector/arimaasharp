
#include "../core/global.h"
#include "../board/btypes.h"
#include "../board/offsets.h"

const int BTypesInternal::STEP_OFFSET_FROM_OFFSET[33] =
{0x00,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0x08,  0,  0x80,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0x88};

#define ERRL ERRLOC
const int8_t BTypesInternal::DEST_FROM_STEP[256] =
{
ERRL,ERRL,ERRL,ERRL,ERRL,ERRL,ERRL,ERRL,ERRL,0x00,0x01,0x02,0x03,0x04,0x05,0x06,
0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,ERRL,0x10,0x11,0x12,0x13,0x14,0x15,0x16,
0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,ERRL,0x20,0x21,0x22,0x23,0x24,0x25,0x26,
0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,ERRL,0x30,0x31,0x32,0x33,0x34,0x35,0x36,
0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,ERRL,0x40,0x41,0x42,0x43,0x44,0x45,0x46,
0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,ERRL,0x50,0x51,0x52,0x53,0x54,0x55,0x56,
0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,ERRL,0x60,0x61,0x62,0x63,0x64,0x65,0x66,
0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,ERRL,0x70,0x71,0x72,0x73,0x74,0x75,0x76,
0x01,0x02,0x03,0x04,0x05,0x06,0x07,ERRL,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
0x11,0x12,0x13,0x14,0x15,0x16,0x17,ERRL,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
0x21,0x22,0x23,0x24,0x25,0x26,0x27,ERRL,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
0x31,0x32,0x33,0x34,0x35,0x36,0x37,ERRL,0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,
0x41,0x42,0x43,0x44,0x45,0x46,0x47,ERRL,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,
0x51,0x52,0x53,0x54,0x55,0x56,0x57,ERRL,0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
0x61,0x62,0x63,0x64,0x65,0x66,0x67,ERRL,0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
0x71,0x72,0x73,0x74,0x75,0x76,0x77,ERRL,ERRL,ERRL,ERRL,ERRL,ERRL,ERRL,ERRL,ERRL,
};

#define T true
#define F false
const bool BTypesInternal::RABBIT_STEP_VALID[2][256] = {
{
T,T,T,T,T,T,T,T, T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T, T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T, T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T, T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T, T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T, T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T, T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T, T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T, F,F,F,F,F,F,F,F,
T,T,T,T,T,T,T,T, F,F,F,F,F,F,F,F,
T,T,T,T,T,T,T,T, F,F,F,F,F,F,F,F,
T,T,T,T,T,T,T,T, F,F,F,F,F,F,F,F,
T,T,T,T,T,T,T,T, F,F,F,F,F,F,F,F,
T,T,T,T,T,T,T,T, F,F,F,F,F,F,F,F,
T,T,T,T,T,T,T,T, F,F,F,F,F,F,F,F,
T,T,T,T,T,T,T,T, F,F,F,F,F,F,F,F,
},
{
F,F,F,F,F,F,F,F, T,T,T,T,T,T,T,T,
F,F,F,F,F,F,F,F, T,T,T,T,T,T,T,T,
F,F,F,F,F,F,F,F, T,T,T,T,T,T,T,T,
F,F,F,F,F,F,F,F, T,T,T,T,T,T,T,T,
F,F,F,F,F,F,F,F, T,T,T,T,T,T,T,T,
F,F,F,F,F,F,F,F, T,T,T,T,T,T,T,T,
F,F,F,F,F,F,F,F, T,T,T,T,T,T,T,T,
F,F,F,F,F,F,F,F, T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T, T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T, T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T, T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T, T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T, T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T, T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T, T,T,T,T,T,T,T,T,
T,T,T,T,T,T,T,T, T,T,T,T,T,T,T,T,
}
};

