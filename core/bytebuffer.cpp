/*
 * bytebuffer.cpp
 * Author: davidwu
 */

#include <cstring>
#include "../core/global.h"
#include "../core/bytebuffer.h"

ByteBuffer::ByteBuffer(int startLen, bool autoEx)
{
  if(startLen < 0)
    Global::fatalError("ByteBuffer: startLen < 0");
  bytes = new uint8_t[startLen];
  len = startLen;
  numBytes = 0;
  autoExpand = autoEx;
}

ByteBuffer::ByteBuffer(const ByteBuffer& other)
{
  bytes = new uint8_t[other.len];
  len = other.len;
  numBytes = other.numBytes;
  autoExpand = other.autoExpand;

  std::memcpy(bytes,other.bytes,len);
}

ByteBuffer::~ByteBuffer()
{
  delete[] bytes;
}

void ByteBuffer::resizeAtLeast(int newSizeAtLeast)
{
  if(newSizeAtLeast <= len)
    return;
  int newBufferLen = len * 3 / 2 + 64 + max(0,newSizeAtLeast - len);
  resize(newBufferLen);
}

void ByteBuffer::resizeIfTooSmall(int desiredSize)
{
  if(desiredSize <= len)
    return;
  resize(desiredSize);
}

void ByteBuffer::resize(int newSize)
{
  int newBufferLen = newSize;
  if(newBufferLen == len)
    return;

  uint8_t* newBuffer = new uint8_t[newBufferLen];
  if(newBufferLen < len)
    std::memcpy(newBuffer,bytes,newBufferLen);
  else
    std::memcpy(newBuffer,bytes,len);

  uint8_t* oldBuffer = bytes;
  len = newBufferLen;
  bytes = newBuffer;

  if(numBytes > len)
    numBytes = len;

  delete[] oldBuffer;
}

void ByteBuffer::clear()
{
  numBytes = 0;
}

void ByteBuffer::setAutoExpand(bool b)
{
  autoExpand = b;
}

void ByteBuffer::add8(uint8_t x)
{
  if(numBytes + 1 > len)
  {
    if(autoExpand)
      resizeAtLeast(numBytes + 1);
    else
      Global::fatalError("ByteBuffer::add8 overflowed and not autoexpanding");
  }
  bytes[numBytes++] = (uint8_t)(x & 0xFF);
}

void ByteBuffer::add32(int x)
{
  if(numBytes + 4 > len)
  {
    if(autoExpand)
      resizeAtLeast(numBytes + 4);
    else
      Global::fatalError("ByteBuffer::add32 overflowed and not autoexpanding");
  }
  bytes[numBytes++] = (uint8_t)(x & 0xFF);
  bytes[numBytes++] = (uint8_t)((x >> 8) & 0xFF);
  bytes[numBytes++] = (uint8_t)((x >> 16) & 0xFF);
  bytes[numBytes++] = (uint8_t)((x >> 24) & 0xFF);
}

void ByteBuffer::add64(uint64_t x)
{
  if(numBytes + 8 > len)
  {
    if(autoExpand)
      resizeAtLeast(numBytes + 8);
    else
      Global::fatalError("ByteBuffer::add64 overflowed and not autoexpanding");
  }
  bytes[numBytes++] = (uint8_t)(x & 0xFF);
  bytes[numBytes++] = (uint8_t)((x >> 8) & 0xFF);
  bytes[numBytes++] = (uint8_t)((x >> 16) & 0xFF);
  bytes[numBytes++] = (uint8_t)((x >> 24) & 0xFF);
  bytes[numBytes++] = (uint8_t)((x >> 32) & 0xFF);
  bytes[numBytes++] = (uint8_t)((x >> 40) & 0xFF);
  bytes[numBytes++] = (uint8_t)((x >> 48) & 0xFF);
  bytes[numBytes++] = (uint8_t)((x >> 56) & 0xFF);
}

