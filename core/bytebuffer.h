/*
 * bytebuffer.h
 * Author: davidwu
 */

#ifndef BYTEBUFFER_H_
#define BYTEBUFFER_H_

#include "../core/global.h"

class ByteBuffer
{
  public:
  uint8_t* bytes;
  int len;
  int numBytes;
  bool autoExpand;

  ByteBuffer(int startLen, bool autoExpand);
  ByteBuffer(const ByteBuffer& other);
  ~ByteBuffer();

  void clear();
  void setAutoExpand(bool b);

  void add8(uint8_t x);
  void add32(int x);
  void add64(uint64_t x);

  //Resize to exactly this size
  void resize(int newSize);
  //Resize only if less than the desired size, and if so, resize to exactly that.
  void resizeIfTooSmall(int desiredSize);
  //Resize only if less than the desired size, and if so, potentially over-size by a constant factor
  void resizeAtLeast(int newSizeAtLeast);

  private:
  void operator=(const ByteBuffer& other);
};


#endif
