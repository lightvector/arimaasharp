
/*
 * setup.h
 * Author: davidwu
 */

#ifndef SETUP_H
#define SETUP_H

#include "../core/global.h"
#include "../board/board.h"

namespace Setup
{
  static const int SETUP_NORMAL = 0;
  static const int SETUP_RATED_RANDOM = 1;
  static const int SETUP_PARTIAL_RANDOM = 2;
  static const int SETUP_RANDOM = 3;

  void setup(Board& b, int type, uint64_t seed);

  void setupNormal(Board& b, uint64_t seed);

  void setupRatedRandom(Board& b, uint64_t seed);

  void setupPartialRandom(Board& b, uint64_t seed);

  void setupRandom(Board& b, uint64_t seed);

  void testSetupVariability();
}

#endif
