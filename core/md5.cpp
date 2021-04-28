/*
 * md5.cpp
 * Implementation from wikipedia
 *
 * Author: davidwu
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../core/md5.h"

using namespace std;

// leftrotate function definition
#define LEFTROTATE(x, c) (((x) << (c)) | ((x) >> (32 - (c))))

static uint32_t reverse(uint32_t x)
{
  x = (x >> 16) | (x << 16);
  return ((x & 0xff00ff00UL) >> 8) | ((x & 0x00ff00ffUL) << 8);
}

// Note: All variables are unsigned 32 bit and wrap modulo 2^32 when calculating
// r specifies the per-round shift amounts
static const uint32_t r[] =
{7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

static const uint32_t k[] = {
 0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
 0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
 0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
 0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};

void MD5::get(const uint8_t *initial_msg, size_t initial_len, uint32_t hash[4])
{
  // Use binary integer part of the sines of integers (in radians) as constants
  // Initialize variables:
  uint32_t h0, h1, h2, h3;
  h0 = 0x67452301;
  h1 = 0xefcdab89;
  h2 = 0x98badcfe;
  h3 = 0x10325476;

  // Pre-processing: adding a single 1 bit
  //append "1" bit to message
  /* Notice: the input bytes are considered as bits strings,
     where the first bit is the most significant bit of the byte.[37] */

  // Pre-processing: padding with zeros
  //append "0" bit until message length in bit = 448 (mod 512)
  //append length mod (2 pow 64) to message

  int new_len;
  for(new_len = initial_len*8 + 1; new_len%512!=448; new_len++);
  new_len /= 8;

  uint8_t* msg = (uint8_t*)calloc(new_len + 64, 1); // also appends "0" bits
                                 // (we alloc also 64 extra bytes...)
  memcpy(msg, initial_msg, initial_len);
  msg[initial_len] = 128; // write the "1" bit

  uint32_t bits_len = 8*initial_len;   // note, we append the len
  memcpy(msg + new_len, &bits_len, 4); // in bits at the end of the buffer

  // Process the message in successive 512-bit chunks:
  //for each 512-bit chunk of message:
  int offset;
  for(offset=0; offset<new_len; offset += (512/8))
  {
    // break chunk into sixteen 32-bit words w[j], 0 = j = 15
    uint32_t *w = (uint32_t *) (msg + offset);

    // Initialize hash value for this chunk:
    uint32_t a = h0;
    uint32_t b = h1;
    uint32_t c = h2;
    uint32_t d = h3;

    // Main loop:
    uint32_t i;
    for(i = 0; i<64; i++)
    {
      uint32_t f, g;

      if (i < 16) {
          f = (b & c) | ((~b) & d);
          g = i;
      } else if (i < 32) {
          f = (d & b) | ((~d) & c);
          g = (5*i + 1) % 16;
      } else if (i < 48) {
          f = b ^ c ^ d;
          g = (3*i + 5) % 16;
      } else {
          f = c ^ (b | (~d));
          g = (7*i) % 16;
      }

      uint32_t temp = d;
      d = c;
      c = b;
      b = b + LEFTROTATE((a + f + k[i] + w[g]), r[i]);
      a = temp;
    }

    // Add this chunk's hash to result so far:
    h0 += a;
    h1 += b;
    h2 += c;
    h3 += d;
  }

  // cleanup
  free(msg);

  hash[3] = reverse(h0);
  hash[2] = reverse(h1);
  hash[1] = reverse(h2);
  hash[0] = reverse(h3);
}

void MD5::get(const uint8_t *msg, size_t len, uint64_t hash[2])
{
  uint32_t buf[4];
  get(msg,len,buf);
  hash[0] = (uint64_t)buf[0] | ((uint64_t)buf[1] << 32);
  hash[1] = (uint64_t)buf[2] | ((uint64_t)buf[3] << 32);
}

void MD5::get(const uint8_t *msg, size_t len, uint8_t hash[16])
{
  uint32_t buf[4];
  get(msg,len,buf);
  for(int i = 0; i<4; i++)
  {
    hash[i*4+0] = (uint8_t)(buf[i] & 0xFF);
    hash[i*4+1] = (uint8_t)((buf[i] >> 8) & 0xFF);
    hash[i*4+2] = (uint8_t)((buf[i] >> 16) & 0xFF);
    hash[i*4+3] = (uint8_t)((buf[i] >> 24) & 0xFF);
  }
}
static const char* hexChars = "0123456789abcdef";
void MD5::get(const uint8_t *msg, size_t len, char hash[33])
{
  uint32_t buf[4];
  get(msg,len,buf);
  for(int i = 0; i<4; i++)
  {
    hash[i*8] = hexChars[(buf[3-i] >> 28) & 0xF];
    hash[i*8+1] = hexChars[(buf[3-i] >> 24) & 0xF];
    hash[i*8+2] = hexChars[(buf[3-i] >> 20) & 0xF];
    hash[i*8+3] = hexChars[(buf[3-i] >> 16) & 0xF];
    hash[i*8+4] = hexChars[(buf[3-i] >> 12) & 0xF];
    hash[i*8+5] = hexChars[(buf[3-i] >> 8) & 0xF];
    hash[i*8+6] = hexChars[(buf[3-i] >> 4) & 0xF];
    hash[i*8+7] = hexChars[buf[3-i] & 0xF];
  }
  hash[32] = '\0';
}

void MD5::get(const char* msg, char hash[33]) {return get((const uint8_t*)msg,strlen(msg),hash);}
void MD5::get(const char* msg, uint8_t hash[16]) {return get((const uint8_t*)msg,strlen(msg),hash);}
void MD5::get(const char* msg, uint32_t hash[4]) {return get((const uint8_t*)msg,strlen(msg),hash);}
void MD5::get(const char* msg, uint64_t hash[2]) {return get((const uint8_t*)msg,strlen(msg),hash);}

#define CONVERTMSG32 \
  uint8_t* msg2 = new uint8_t[len*4]; \
  for(size_t i = 0; i<len; i++) { \
    msg2[i*4+0] = (uint8_t)(msg[i] & 0xFF); \
    msg2[i*4+1] = (uint8_t)((msg[i] >> 8) & 0xFF); \
    msg2[i*4+2] = (uint8_t)((msg[i] >> 16) & 0xFF); \
    msg2[i*4+3] = (uint8_t)((msg[i] >> 24) & 0xFF); \
  } \
  get(msg2,len*4,hash); \
  delete[] msg2;

void MD5::get(const uint32_t* msg, size_t len, char hash[33]) {CONVERTMSG32}
void MD5::get(const uint32_t* msg, size_t len, uint8_t hash[16]) {CONVERTMSG32}
void MD5::get(const uint32_t* msg, size_t len, uint32_t hash[4]) {CONVERTMSG32}
void MD5::get(const uint32_t* msg, size_t len, uint64_t hash[2]) {CONVERTMSG32}

Hash128 MD5::get(const uint8_t* msg, size_t len)
{
  uint64_t buf[2];
  get(msg,len,buf);
  return Hash128(buf[0],buf[1]);
}
Hash128 MD5::get(const char* msg)
{
  uint64_t buf[2];
  get(msg,buf);
  return Hash128(buf[0],buf[1]);
}
Hash128 MD5::get(const uint32_t* msg, size_t len)
{
  uint64_t buf[2];
  get(msg,len,buf);
  return Hash128(buf[0],buf[1]);
}

