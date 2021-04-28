
#include <ctime>
#include "rand.h"
#include "timer.h"
#include "global.h"
#include "hash.h"

static int inits = 0;
Rand::Rand()
{
  init();
}

Rand::Rand(int seed)
{
  init((uint64_t)seed);
}

Rand::Rand(uint64_t seed)
{
  init(seed);
}

Rand::~Rand()
{

}

void Rand::init()
{
  //Note that inits++ is not threadsafe
  uint64_t seed0 = (uint64_t)time(NULL);
  uint64_t seed1 = Hash::jenkinsMixSingle(
      (~ time(NULL)) * 69069 + 12345,
      0x7331BABEU ^ (uint32_t)clock(),
      0xFEEDCAFEU + (inits++) + (uint32_t)ClockTimer::getPrecisionSystemTime());
  init(seed0 + (seed1 << 32));
}

void Rand::init(uint64_t seed)
{
  initSeed = seed;
  z = w = j = c = 0;

  //Seed linear congruential
  seed = Hash::basicLCong(Hash::murmurMix(seed));
  c = Hash::highBits(seed) ^ Hash::lowBits(seed);

  //Seed xorshift, avoiding bad value 0
  seed = Hash::basicLCong(seed);
  j = 1U + (uint32_t)(Hash::murmurMix(seed) % 0xFFFFFFFEU);

  //Seed mini mwc, avoiding bad period 1 values
  //z: 0, 0x9068ffffU
  seed = Hash::basicLCong(seed);
  z = 1U + (uint32_t)(Hash::murmurMix(seed) % 0xFFFFFFFDU);
  if(z >= 0x9068ffffU) z++;

  //Seed mini mwc, avoiding bad period 1 values
  //w: 0, 0x464fffffU, 0x8c9ffffeU, 0xd2effffdU
  seed = Hash::basicLCong(seed);
  w = 1U + (uint32_t)(Hash::murmurMix(seed) % 0xFFFFFFFBU);
  if(w >= 0x464fffffU) w++;
  if(w >= 0x8c9ffffeU) w++;
  if(w >= 0xd2effffdU) w++;

  //Seed the long cwmc. Technically we should check that this is seeded nonzero, but that's unlikely
  //Each entry should be at most 0xFFFFFFFE
  for(int i = 0; i<CMWC_LEN; i++)
  {
    seed = Hash::basicLCong(seed);
    q[i] = (uint32_t)((nextUIntRaw() ^ Hash::murmurMix(seed)) % 0xFFFFFFFFU);
  }

  //Initialize the carry
  seed = Hash::basicLCong(seed);
  q_c = (uint32_t)(Hash::murmurMix(seed) % CMWC_MULT);

  //Initialize location in the long cmwc
  q_idx = 0;

  //Initialize the long xorshift* generator. Technically we should again check that this is seeded nonzero
  //but that's unlikely
  a_idx = 0;
  for(int i = 0; i<XORMULT_LEN; i++)
  {
    seed = Hash::basicLCong(seed);
    a[i] = Hash::murmurMix(seed + nextUIntRaw());
  }

  //Run the generator a little while to mix things up
  for(int i = 0; i<150; i++)
    nextUInt();

  hasGaussian = false;
  storedGaussian = 0.0;
  numCalls = 0;
}

