
/*
 * bitmap.h
 * Author: davidwu
 *
 * Basic 64-bit bitmap class for an Arimaa board.
 */

#ifndef BITMAP_H
#define BITMAP_H

#include "../core/global.h"
#include "../board/btypes.h"
using namespace std;

static const int nextBitArr[64] =
{
  63, 30,  3, 32, 59, 14, 11, 33,
  60, 24, 50,  9, 55, 19, 21, 34,
  61, 29,  2, 53, 51, 23, 41, 18,
  56, 28,  1, 43, 46, 27,  0, 35,
  62, 31, 58,  4,  5, 49, 54,  6,
  15, 52, 12, 40,  7, 42, 45, 16,
  25, 57, 48, 13, 10, 39,  8, 44,
  20, 47, 38, 22, 17, 37, 36, 26
};

struct Bitmap
{
  public:
  uint64_t bits;

  static const Bitmap BMPZEROS;
  static const Bitmap BMPONES;
  static const Bitmap BMPTRAPS;
  static const Bitmap BMPBEHINDTRAPS;

  //Columns
  static const Bitmap BMPX[8];
  //Rows
  static const Bitmap BMPY[8];

  inline Bitmap()
  :bits(0)
  {}

  inline Bitmap(uint64_t b)
  :bits(b)
  {}

  inline static Bitmap makeLoc(int k)
  {return Bitmap((uint64_t)1 << gIdx(k));}

  inline bool operator==(const Bitmap b) const
  {return bits == b.bits;}

  inline bool operator!=(const Bitmap b) const
  {return bits != b.bits;}

  inline static Bitmap shiftS(const Bitmap b)
  {return Bitmap(b.bits >> 8);}

  inline static Bitmap shiftN(const Bitmap b)
  {return Bitmap(b.bits << 8);}

  inline static Bitmap shiftW(const Bitmap b)
  {return Bitmap((b.bits & (uint64_t)0xFEFEFEFEFEFEFEFEULL) >> 1);}

  inline static Bitmap shiftE(const Bitmap b)
  {return Bitmap((b.bits & (uint64_t)0x7F7F7F7F7F7F7F7FULL) << 1);}

  inline static Bitmap shiftG(const Bitmap b, pla_t pla)
  {return pla == GOLD ? shiftN(b) : shiftS(b);}

  inline static Bitmap shiftF(const Bitmap b, pla_t pla)
  {return pla == GOLD ? shiftS(b) : shiftN(b);}

  inline const Bitmap operator&(const Bitmap b) const
  {return Bitmap(bits & b.bits);}

  inline const Bitmap operator|(const Bitmap b) const
  {return Bitmap(bits | b.bits);}

  inline const Bitmap operator^(const Bitmap b) const
  {return Bitmap(bits ^ b.bits);}

  inline const Bitmap operator~() const
  {return Bitmap(~bits);}

  inline Bitmap& operator&=(const Bitmap b)
  {bits &= b.bits; return *this;}

  inline Bitmap& operator|=(const Bitmap b)
  {bits |= b.bits; return *this;}

  inline Bitmap& operator^=(const Bitmap b)
  {bits ^= b.bits; return *this;}

  inline static Bitmap adjWE(const Bitmap b)
  {
    return Bitmap(
      ((b.bits & (uint64_t)0xFEFEFEFEFEFEFEFEULL) >> 1) |
      ((b.bits & (uint64_t)0x7F7F7F7F7F7F7F7FULL) << 1)
      );
  }

  inline static Bitmap adjNS(const Bitmap b)
  {
    return Bitmap(
        (b.bits >> 8) |
        (b.bits << 8)
      );
  }

  inline static Bitmap adj(const Bitmap b)
  {
    return Bitmap(
      (b.bits >> 8) |
      (b.bits << 8) |
      ((b.bits & (uint64_t)0xFEFEFEFEFEFEFEFEULL) >> 1) |
      ((b.bits & (uint64_t)0x7F7F7F7F7F7F7F7FULL) << 1)
      );
  }

  inline static Bitmap adj2(const Bitmap b)
  {
    return Bitmap(
      (((b.bits & (uint64_t)0xFEFEFEFEFEFEFEFEULL) >> 1) & (
         (b.bits >> 8) |
         (b.bits << 8) |
         ((b.bits & (uint64_t)0x7F7F7F7F7F7F7F7FULL) << 1)
      )) |
      (((b.bits & (uint64_t)0x7F7F7F7F7F7F7F7FULL) << 1) & (
         (b.bits >> 8) |
         (b.bits << 8)
      )) |
      ((b.bits >> 8) & (b.bits << 8))
      );
  }

  inline bool isOne(int index) const
  {return (int)(bits >> gIdx(index)) & 0x1;}

  inline void toggle(int index)
  {bits = bits ^ ((uint64_t)1 << gIdx(index));}

  inline void setOff(int index)
  {bits = bits & (~((uint64_t)1 << gIdx(index)));}

  inline void setOn(int index)
  {bits = bits | ((uint64_t)1 << gIdx(index));}

  inline bool hasBits() const
  {return bits != 0;}

  inline bool isEmpty() const
  {return bits == 0;}

  inline bool isSingleBit() const
  {return bits != 0 && (bits & (bits-1)) == 0;}

  inline bool isEmptyOrSingleBit() const
  {return (bits & (bits-1)) == 0;}

  inline static Bitmap hFlip(const Bitmap b)
  {
    uint64_t bits = b.bits;
    bits = ((bits & 0x0F0F0F0F0F0F0F0FULL) << 4) | ((bits & 0xF0F0F0F0F0F0F0F0ULL) >> 4);
    bits = ((bits & 0x3333333333333333ULL) << 2) | ((bits & 0xCCCCCCCCCCCCCCCCULL) >> 2);
    bits = ((bits & 0x5555555555555555ULL) << 1) | ((bits & 0xAAAAAAAAAAAAAAAAULL) >> 1);
    return Bitmap(bits);
  }

  inline static Bitmap vFlip(const Bitmap b)
  {
    uint64_t bits = b.bits;
    bits = ((bits & 0x00000000FFFFFFFFULL) << 32) | ((bits & 0xFFFFFFFF00000000ULL) >> 32);
    bits = ((bits & 0x0000FFFF0000FFFFULL) << 16) | ((bits & 0xFFFF0000FFFF0000ULL) >> 16);
    bits = ((bits & 0x00FF00FF00FF00FFULL) <<  8) | ((bits & 0xFF00FF00FF00FF00ULL) >>  8);
    return Bitmap(bits);
  }

  //Sets dest = dest | src, then src = 0
  //Warning: note what happens if src = dest.
  inline void transfer(int src, int dest)
  {
    src = gIdx(src);
    dest = gIdx(dest);
    bits |= (uint64_t)((bits >> src) & 0x1) << dest;
    bits &= ~((uint64_t)1 << src);
  }

  // http://chessprogramming.wikispaces.com/BitScan
  inline int nextBit()
  {
    //int bit =  __builtin_ctzll(bits);
    uint64_t b = bits ^ (bits-1);
    uint32_t folded = (uint32_t)(b ^ (b >> 32));
    int bit = nextBitArr[(folded * 0x78291ACF) >> 26];
    bits &= (bits-1);
    return gLoc(bit);
  }

  inline int countBitsIterative() const
  {
    uint64_t v = bits;
    int count = 0;
    while (v)
    {
       count++;
       v &= v - 1;
    }
    return count;
  }

  inline int countBits() const
  {
#ifdef DO_USE_POPCOUNT
    return __builtin_popcountll(bits);
#else
    uint64_t x =  bits - ((bits >> 1) & 0x5555555555555555ULL);
    x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
    x = (x + (x >> 4)) & 0x0f0f0f0f0f0f0f0fULL;
    x = (x * 0x0101010101010101ULL) >> 56;
    return (int)x;
#endif
  }

  inline void print(ostream& out) const
  {
    for(int y = 7; y>=0; y--)
    {
      for(int x = 0; x<8; x++)
      {
        out << (isOne(gLoc(x,y)) ? 1 : 0);
      }
      out << endl;
    }
  }

  inline friend ostream& operator<<(ostream& out, const Bitmap& map)
  {
    map.print(out);
    return out;
  }

};


#endif






