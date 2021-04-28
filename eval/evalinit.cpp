
/*
 * evalinit.cpp
 * Author: davidwu
 */

#include "../eval/eval.h"
#include "../eval/internal.h"

void Eval::init()
{
  initMaterial();
  initLogistic();
  Logistic::init();
  RabGoal::init();
}


