
/*
 * rand.h
 * Author: davidwu
 *
 * Basic class for random number generation.
 * Not threadsafe!
 * Note: Signed integer functions might not be portable to other architectures.
 *
 * Combines:
 * 2 multiply-with-carry generators (32 bits state),
 * xorshift generator (32 bits state)
 * linear congruential generator (32 bits state),
 * complementary multiply-with-carry generator (2048+32 bits state)
 */

#ifndef RAND_H
#define RAND_H

#include <cassert>
#include <cmath>
#include <stdint.h>
#include <iostream>

using namespace std;

class Rand
{
  private:
  static const int CMWC_LEN = 64;
  static const int CMWC_MASK = CMWC_LEN-1;
  static const uint64_t CMWC_MULT = 987651206ULL;
  static const int XORMULT_LEN = 16;
  static const int XORMULT_MASK = XORMULT_LEN-1;

  uint32_t z; //MWC1
  uint32_t w; //MWC2
  uint32_t j; //Xorshift
  uint32_t c; //LCONG

  uint32_t q[CMWC_LEN]; //Long CMWC, entries must be modulo [0,2^31-1)
  uint32_t q_c;         //Long CMWC constant add, must be [0,CMWC_MULT)
  int q_idx;            //Long CMWC index, [0,CMWC_LEN)

  uint64_t a[XORMULT_LEN]; //Long XORSHIFT multiplication generator (xorshift1024*)
  uint64_t a_idx;

  bool hasGaussian;
  double storedGaussian;

  uint64_t initSeed;
  uint64_t numCalls;

  public:

  //Initializes according to system time and some other unique junk, tries
  //to make sure multiple invocations will be different
  //Note if two *different* threads calling this in rapid succession, the seeds
  //might *not* be unique.
  Rand();

  //Intializes according to the provided seed
  Rand(int seed);

  //Intializes according to the provided seed
  Rand(uint64_t seed);

  //Reinitialize according to system time and some other unique junk
  void init();

  //Reinitialize according to the provided seed
  void init(uint64_t seed);

  ~Rand();

  //HELPERS---------------------------------------------
  private:

  //Generate a random value without the CMWC
  uint32_t nextUIntRaw();

  public:

  //SEED RETRIEVAL---------------------------------------

  uint64_t getSeed() const;

  uint64_t getNumCalls() const;

  //UNSIGNED INTEGER-------------------------------------

  //Returns a random integer in [0,2^32)
  uint32_t nextUInt();

  //Returns a random integer in [0,n)
  uint32_t nextUInt(uint32_t n);

  //Returns a random integer according to the given frequency distribution
  uint32_t nextUInt(const int* freq, size_t n);

  //SIGNED INTEGER---------------------------------------

  //Returns a random integer in [-2^31,2^31)
  int32_t nextInt();

  //Returns a random integer in [a,b]
  int32_t nextInt(int32_t a, int32_t b);

  //64-BIT INTEGER--------------------------------------

  //Returns a random integer in [0,2^64)
  uint64_t nextUInt64();

  //Returns a random integer in [0,n)
  uint64_t nextUInt64(uint64_t n);

  //DOUBLE----------------------------------------------

  //Returns a random double in [0,1)
  double nextDouble();

  //Returns a random double in [0,n). Note: Rarely, it may be possible for n to occur.
  double nextDouble(double n);

  //Returns a random double in [a,b). Note: Rarely, it may be possible for b to occur.
  double nextDouble(double a, double b);

  //Returns a normally distributed double with mean 0 stdev 1
  double nextGaussian();

  //Returns a logistically distributed double with mean 0 and scale 1 (cdf = 1/(1+exp(-x)))
  double nextLogistic();
};

inline uint64_t Rand::getSeed() const
{
  return initSeed;
}

inline uint64_t Rand::getNumCalls() const
{
  return numCalls;
}

inline uint32_t Rand::nextUIntRaw()
{
  z = 36969 * (z & 0xFFFF) + (z >> 16);
  w = 18000 * (w & 0xFFFF) + (w >> 16);
  j ^= j << 13;
  j ^= j >> 17;
  j ^= j << 5;
  c = 69069*c + 0x1372B295;

  return (((z<<16)+w)^c)+j;
}

inline uint32_t Rand::nextUInt()
{
  //t = multiplier * state + carry
  uint64_t t = CMWC_MULT * q[q_idx] + (uint64_t)q_c;

  //Compute x = t mod (2^32-1) and new_carry = t / (2^32-1)
  //First divide by 2^32. This is how many even groups of 2^32 we have.
  uint32_t new_carry = (uint32_t)(t >> 32);
  //Now x = (uint32_t)t is the leftovers. By adding the number of even groups of 2^32, we're computing what
  //the leftovers would be if we had groups of 2^32 - 1 instead.
  uint32_t x = (uint32_t)t+new_carry;
  //If we overflowed on that addition (x+1 in [1,new_carry]), then we lost a 2^32.
  //Increment x so that we lost a 2^32 - 1 instead and then add that as a new even group of 2^32 - 1
  //Otherwise, if we didn't overflow but we're exactly 2^32 - 1 (x+1 == 0),
  //then again increment x so that it's equal to 0, and pull that off as a new even group of 2^32 - 1.
  if(x+1 <= new_carry) {x++; new_carry++;}
  q_c = new_carry;

  uint32_t m = 0xFFFFFFFEU;
  uint32_t q_result = m-x;
  q[q_idx] = q_result;

  q_idx = (q_idx + 1) & CMWC_MASK;

  //xorshift1024* generator
  uint64_t a0 = a[a_idx];
  uint64_t a1 = a[a_idx = (a_idx + 1) & XORMULT_MASK];
  a1 ^= a1 << 31; // a
  a1 ^= a1 >> 11; // b
  a0 ^= a0 >> 30; // c
  a[a_idx] = a0 ^ a1;
  uint64_t a_result = a[a_idx] * 1181783497276652981LL;

  return (uint32_t)(a_result >> 32) + (q_result ^ nextUIntRaw());
}


inline uint32_t Rand::nextUInt(uint32_t n)
{
  assert(n > 0);
  uint32_t bits, val;
  do {
    bits = nextUInt();
    val = bits % n;
  } while((uint32_t)(bits - val + (n-1)) < (uint32_t)(bits - val)); //If adding (n-1) overflows, no good.
  return val;
}

inline int32_t Rand::nextInt()
{
  return (int32_t)nextUInt();
}

inline int32_t Rand::nextInt(int32_t a, int32_t b)
{
  assert(b >= a);
  uint32_t max = (uint32_t)b-(uint32_t)a+(uint32_t)1;
  if(max == 0)
    return (int32_t)(nextUInt());
  else
    return (int32_t)(nextUInt(max)+(uint32_t)a);
}

inline uint64_t Rand::nextUInt64()
{
  return ((uint64_t)nextUInt()) | ((uint64_t)nextUInt() << 32);
}

inline uint64_t Rand::nextUInt64(uint64_t n)
{
  assert(n > 0);
  uint64_t bits, val;
  do {
    bits = nextUInt64();
    val = bits % n;
  } while((uint64_t)(bits - val + (n-1)) < (uint64_t)(bits - val)); //If adding (n-1) overflows, no good.
  return val;
}

inline uint32_t Rand::nextUInt(const int* freq, size_t n)
{
  int64_t sum = 0;
  for(uint32_t i = 0; i<n; i++)
  {
    assert(freq[i] >= 0);
    sum += freq[i];
  }

  int64_t r = (int64_t)nextUInt64(sum);
  assert(r >= 0);
  for(uint32_t i = 0; i<n; i++)
  {
    r -= freq[i];
    if(r < 0)
    {return i;}
  }
  //Should not get to here
  assert(false);
  return 0;
}

inline double Rand::nextDouble()
{
  double x;
  do
  {
    uint64_t bits = nextUInt64() & ((1ULL << 53)-1ULL);
    x = (double)bits / (double)(1ULL << 53);
  }
  //Catch loss of precision of long --> double conversion
  while (!(x>=0.0 && x<1.0));

  return x;
}

inline double Rand::nextDouble(double n)
{
  assert(n >= 0);
  return nextDouble()*n;
}

inline double Rand::nextDouble(double a, double b)
{
  assert(b >= a);
  return a+nextDouble(b-a);
}

inline double Rand::nextGaussian()
{
  if(hasGaussian)
  {
    hasGaussian = false;
    return storedGaussian;
  }
  else
  {
    double v1, v2, s;
    do
    {
      v1 = 2 * nextDouble(-1.0,1.0);
      v2 = 2 * nextDouble(-1.0,1.0);
      s = v1 * v1 + v2 * v2;
    } while (s >= 1 || s == 0);

    double multiplier = sqrt(-2 * log(s)/s);
    storedGaussian = v2 * multiplier;
    hasGaussian = true;
    return v1 * multiplier;
  }
}

inline double Rand::nextLogistic()
{
  double x = nextDouble();
  return log(x / (1.0 - x));
}

/*

 R_UC    (unsigned char)

 R_ZNEW  (R_Z=36969*(R_Z&65535)+(R_Z>>16))
 R_WNEW  (R_W=18000*(R_W&65535)+(R_W>>16))

 R_MWC   ((R_ZNEW<<16)+R_WNEW )
 R_SHR3  (R_JSR^=(R_JSR<<17), R_JSR^=(R_JSR>>13), R_JSR^=(R_JSR<<5))
 R_CONG  (R_JCONG=69069*R_JCONG+1234567)
 R_FIB   ((R_B=R_A+R_B),(R_A=R_B-R_A))
 R_KISS  ((R_MWC^R_CONG)+R_SHR3)
 R_LFIB4 (R_C++,R_T[R_C]=R_T[R_C]+R_T[R_UC(R_C+58)]+R_T[R_UC(R_C+119)]+R_T[R_UC(R_C+178)])
 R_SWB   (R_C++,R_BRO=(R_X<R_Y),R_T[R_C]=(R_X=R_T[R_UC(R_C+34)])-(R_Y=R_T[R_UC(R_C+19)]+R_BRO))
 R_UNI   (R_KISS*2.328306e-10)
 R_VNI   (((long)R_KISS)*4.656613e-10)

 RAND32  (R_KISS)

//Global static variables:
static R_UL R_Z=362436069, R_W=521288629, R_JSR=123456789, R_JCONG=380116160;
static R_UL R_A=224466889, R_B=7584631, R_T[256];

//Use random seeds to reset z,w,jsr,jcong,a,b, and the table t[256]
static R_UL R_X=0,R_Y=0,R_BRO;
static unsigned char R_C=0;
*/

/*
//Example procedure to set the table, using KISS:
void setTableExample(R_UL i1,R_UL i2,R_UL i3,R_UL i4,R_UL i5,R_UL i6)
{
  R_Z=i1;
  R_W=i2;
  R_JSR=i3;
  R_JCONG=i4;
  R_A=i5;
  R_B=i6;

  for(int i = 0; i < 256; i++)
  {R_T[i]=R_KISS;}
}

//This is a test main program.  It should compile and print 7  0's.
int main()
{
  int i; R_UL k;
  setTableExample(12345,65435,34221,12345,9983651,95746118);

  for(i=1;i<1000001;i++){k = R_LFIB4;} printf("%lu\n", k-1064612766U);
  for(i=1;i<1000001;i++){k = R_SWB  ;} printf("%lu\n", k- 627749721U);
  for(i=1;i<1000001;i++){k = R_KISS ;} printf("%lu\n", k-1372460312U);
  for(i=1;i<1000001;i++){k = R_CONG ;} printf("%lu\n", k-1529210297U);
  for(i=1;i<1000001;i++){k = R_SHR3 ;} printf("%lu\n", k-2642725982U);
  for(i=1;i<1000001;i++){k = R_MWC  ;} printf("%lu\n", k- 904977562U);
  for(i=1;i<1000001;i++){k = R_FIB  ;} printf("%lu\n", k-3519793928U);

  return 0;
}
*/

/*
  From: George Marsaglia <geo@stat.fsu.edu>
  Subject: Random numbers for C: The END?
  Date: Wed, 20 Jan 1999 10:55:14 -0500
  Newsgroups: sci.stat.math,sci.math

   Any one of KISS, MWC, FIB, LFIB4, SWB, SHR3, or CONG
   can be used in an expression to provide a random 32-bit
   integer.

   The KISS generator, (Keep It Simple Stupid), is
   designed to combine the two multiply-with-carry
   generators in MWC with the 3-shift register SHR3 and
   the congruential generator CONG, using addition and
   exclusive-or. Period about 2^123.
   It is one of my favorite generators.

   The  MWC generator concatenates two 16-bit multiply-
   with-carry generators, x(n)=36969x(n-1)+carry,
   y(n)=18000y(n-1)+carry  mod 2^16, has period about
   2^60 and seems to pass all tests of randomness. A
   favorite stand-alone generator---faster than KISS,
   which contains it.

   FIB is the classical Fibonacci sequence
   x(n)=x(n-1)+x(n-2),but taken modulo 2^32.
   Its period is 3*2^31 if one of its two seeds is odd
   and not 1 mod 8. It has little worth as a RNG by
   itself, but provides a simple and fast component for
   use in combination generators.

   SHR3 is a 3-shift-register generator with period
   2^32-1. It uses y(n)=y(n-1)(I+L^17)(I+R^13)(I+L^5),
   with the y's viewed as binary vectors, L the 32x32
   binary matrix that shifts a vector left 1, and R its
   transpose.  SHR3 seems to pass all except those
   related to the binary rank test, since 32 successive
   values, as binary vectors, must be linearly
   independent, while 32 successive truly random 32-bit
   integers, viewed as binary vectors, will be linearly
   independent only about 29% of the time.

   CONG is a congruential generator with the widely used 69069
   multiplier: x(n)=69069x(n-1)+1234567.  It has period
   2^32. The leading half of its 32 bits seem to pass
   tests, but bits in the last half are too regular.

   LFIB4 is an extension of what I have previously
   defined as a lagged Fibonacci generator:
   x(n)=x(n-r) op x(n-s), with the x's in a finite
   set over which there is a binary operation op, such
   as +,- on integers mod 2^32, * on odd such integers,
   exclusive-or(xor) on binary vectors. Except for
   those using multiplication, lagged Fibonacci
   generators fail various tests of randomness, unless
   the lags are very long. (See SWB below).
   To see if more than two lags would serve to overcome
   the problems of 2-lag generators using +,- or xor, I
   have developed the 4-lag generator LFIB4 using
   addition: x(n)=x(n-256)+x(n-179)+x(n-119)+x(n-55)
   mod 2^32. Its period is 2^31*(2^256-1), about 2^287,
   and it seems to pass all tests---in particular,
   those of the kind for which 2-lag generators using
   +,-,xor seem to fail.  For even more confidence in
   its suitability,  LFIB4 can be combined with KISS,
   with a resulting period of about 2^410: just use
   (KISS+LFIB4) in any C expression.

   SWB is a subtract-with-borrow generator that I
   developed to give a simple method for producing
   extremely long periods:
      x(n)=x(n-222)-x(n-237)- borrow mod 2^32.
   The 'borrow' is 0, or set to 1 if computing x(n-1)
   caused overflow in 32-bit integer arithmetic. This
   generator has a very long period, 2^7098(2^480-1),
   about 2^7578.   It seems to pass all tests of
   randomness, except for the Birthday Spacings test,
   which it fails badly, as do all lagged Fibonacci
   generators using +,- or xor. I would suggest
   combining SWB with KISS, MWC, SHR3, or CONG.
   KISS+SWB has period >2^7700 and is highly
   recommended.
   Subtract-with-borrow has the same local behaviour
   as lagged Fibonacci using +,-,xor---the borrow
   merely provides a much longer period.
   SWB fails the birthday spacings test, as do all
   lagged Fibonacci and other generators that merely
   combine two previous values by means of =,- or xor.
   Those failures are for a particular case: m=512
   birthdays in a year of n=2^24 days. There are
   choices of m and n for which lags >1000 will also
   fail the test.  A reasonable precaution is to always
   combine a 2-lag Fibonacci or SWB generator with
   another kind of generator, unless the generator uses
   *, for which a very satisfactory sequence of odd
   32-bit integers results.

   The classical Fibonacci sequence mod 2^32 from FIB
   fails several tests.  It is not suitable for use by
   itself, but is quite suitable for combining with
   other generators.

   The last half of the bits of CONG are too regular,
   and it fails tests for which those bits play a
   significant role. CONG+FIB will also have too much
   regularity in trailing bits, as each does. But keep
   in mind that it is a rare application for which
   the trailing bits play a significant role.  CONG
   is one of the most widely used generators of the
   last 30 years, as it was the system generator for
   VAX and was incorporated in several popular
   software packages, all seemingly without complaint.

   Finally, because many simulations call for uniform
   random variables in 0<x<1 or -1<x<1, I use #define
   statements that permit inclusion of such variates
   directly in expressions:  using UNI will provide a
   uniform random real (float) in (0,1), while VNI will
   provide one in (-1,1).

   All of these: MWC, SHR3, CONG, KISS, LFIB4, SWB, FIB
   UNI and VNI, permit direct insertion of the desired
   random quantity into an expression, avoiding the
   time and space costs of a function call. I call
   these in-line-define functions.  To use them, static
   variables z,w,jsr,jcong,a and b should be assigned
   seed values other than their initial values.  If
   LFIB4 or SWB are used, the static table t[256] must
   be initialized.

   A note on timing:  It is difficult to provide exact
   time costs for inclusion of one of these in-line-
   define functions in an expression.  Times may differ
   widely for different compilers, as the C operations
   may be deeply nested and tricky. I suggest these
   rough comparisons, based on averaging ten runs of a
   routine that is essentially a long loop:
   for(i=1;i<10000000;i++) L=KISS; then with KISS
   replaced with SHR3, CONG,... or KISS+SWB, etc. The
   times on my home PC, a Pentium 300MHz, in nanoseconds:
   FIB 49;LFIB4 77;SWB 80;CONG 80;SHR3 84;MWC 93;KISS 157;
   VNI 417;UNI 450;


static unsigned long Q[4096], c=123;
unsigned long CMWC(void)
{
  unsigned long long t, a=18782LL;
  static unsignd long i=4095;
  unsigned long x, m=0xFFFFFFFE;

  i = (i + 1) & 4095;
  t = a * Q[i] + c;
  c = (t >> 32);
  x = t + c;
  if(x < c)
  {
    ++x;
    ++c;
  }
  return (Q[i] = m - x);
}

static unsigned long Q[256], c=123;
unsigned long CMWC(void)
{
  unsigned long long t, a=987662290LL;
  static unsigned long i=255;
  unsigned long x, m=0xFFFFFFFE;

  i = (i + 1) & 255;
  t = a * Q[i] + c;
  c = (t >> 32);
  x = t + c;
  if(x < c)
  {
    ++x;
    ++c;
  }
  return (Q[i] = m - x);
}


static unsigned long Q[64], c=123;
unsigned long CMWC(void)
{
  unsigned long long t, a=987651206LL;
  static unsigned long i=63;
  unsigned long x, m=0xFFFFFFFE;

  i = (i + 1) & 63;
  t = a * Q[i] + c;
  c = (t >> 32);
  x = t + c;
  if(x < c)
  {
    ++x;
    ++c;
  }
  return (Q[i] = m - x);
}


static unsigned long Q[16], c=123;
unsigned long CMWC(void)
{
  unsigned long long t, a=987651182LL;
  static unsigned long i=15;
  unsigned long x, m=0xFFFFFFFE;

  i = (i + 1) & 15;
  t = a * Q[i] + c;
  c = (t >> 32);
  x = t + c;
  if(x < c)
  {
    ++x;
    ++c;
  }
  return (Q[i] = m - x);
}


 */


#endif
