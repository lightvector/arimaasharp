/*
 * gengoalpatterns.h
 * Author: davidwu
 */

#ifndef GENGOALPATTERNS_H_
#define GENGOALPATTERNS_H_

#include "../board/board.h"
#include "../pattern/pattern.h"

struct GoalPatternInput
{
  char pattern[64];
};

namespace GenGoalPatterns
{
  bool genGoalPattern(const Pattern& basePat, uint64_t seed, int stepsLeft, loc_t center, int repsPerCond,
      int printVerbosity, Pattern& output);
}



#endif /* GENGOALPATTERNS_H_ */
