
/*
 * searchflags.h
 * Author: davidwu
 */

#ifndef SEARCHFLAGS_H_
#define SEARCHFLAGS_H_

#include "../core/global.h"

typedef int8_t flag_t;

namespace Flags
{
  const flag_t FLAG_NONE = 0;  //Indicates a null value of some sort
  const flag_t FLAG_ALPHA = 1; //Indicates that the true eval <= the reported eval
  const flag_t FLAG_BETA = 2;  //Indicates that the true eval >= the reported eval
  const flag_t FLAG_EXACT = 3; //Indicates that the true eval == the reported eval

  const int NUMFLAGS = 4;

  extern const char* FLAG_NAMES[4];
}


#endif
