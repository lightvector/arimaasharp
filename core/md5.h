/*
 * md5.h
 *
 *  Created on: Dec 20, 2012
 *      Author: davidwu
 */

#ifndef MD5_H_
#define MD5_H_

#include <stdint.h>
#include "../core/hash.h"

namespace MD5
{
  //Output endianness------------------------------------------------------------
  //The numerical outputs are "little-endian" in the sense that if you sequentially
  //walked through the hash in reverse and wrote each value in hex, you would
  //get the standard digest. (within each individual hash array elt, of course,
  //it's still dependent on the architecture).
  //For char output, it's precisely the digest string, with a null terminator.
  //Compared to the other outputs, this behaves like "big endian".

  //Input endianness------------------------------------------------------------
  //The char and uint8_t inputs behave identically, giving you the appropriate hash
  //of the given char or byte stream.
  //The uint32_t inputs are "little-endian", the first byte in the stream fed to the
  //hash is the least significant byte of the first integer in the stream.
  void get(const char* msg, char hash[33]);
  void get(const char* msg, uint8_t hash[16]);
  void get(const char* msg, uint32_t hash[4]);
  void get(const char* msg, uint64_t hash[2]);
  Hash128 get(const char* msg);

  void get(const uint8_t* msg, size_t len, char hash[33]);
  void get(const uint8_t* msg, size_t len, uint8_t hash[16]);
  void get(const uint8_t* msg, size_t len, uint32_t hash[4]);
  void get(const uint8_t* msg, size_t len, uint64_t hash[2]);
  Hash128 get(const uint8_t* msg, size_t len);

  void get(const uint32_t* msg, size_t len, char hash[33]);
  void get(const uint32_t* msg, size_t len, uint8_t hash[16]);
  void get(const uint32_t* msg, size_t len, uint32_t hash[4]);
  void get(const uint32_t* msg, size_t len, uint64_t hash[2]);
  Hash128 get(const uint32_t* msg, size_t len);
}


#endif /* MD5_H_ */
