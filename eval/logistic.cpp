/*
 * logistic.cpp
 * Author: davidwu
 */

#include <cmath>
#include "../core/global.h"
#include "../eval/internal.h"

int Logistic::LOGISTIC_ARR20[255];
int Logistic::LOGISTIC_ARR16[255];
int Logistic::LOGISTIC_INTEGRAL_ARR[255];

void Logistic::init()
{
  for(int i = 0; i<255; i++)
  {
    int x = i-127;
    LOGISTIC_ARR20[i] = (int)(1024.0/(1.0+exp(-x/20.0)));
    LOGISTIC_ARR16[i] = (int)(1024.0/(1.0+exp(-x/16.0)));
    LOGISTIC_INTEGRAL_ARR[i] = (int)(1024.0*log(1.0+exp(x/20.0)));
  }
}

int Logistic::prop20(int x)
{
  int i = x+127;
  if(i < 0)   return LOGISTIC_ARR20[0];
  if(i > 254) return LOGISTIC_ARR20[254];
  return LOGISTIC_ARR20[i];
}

int Logistic::prop16(int x)
{
  int i = x+127;
  if(i < 0)   return LOGISTIC_ARR16[0];
  if(i > 254) return LOGISTIC_ARR16[254];
  return LOGISTIC_ARR16[i];
}
