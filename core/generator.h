/*
 * generator.h
 * Author: davidwu
 */

#ifndef GENERATOR_H_
#define GENERATOR_H_

//Implements "yield" functionality of python.
//Intended to be used in the implementation of the next() function of an "iterator" type object,
//where genInternalState and all other relevant state to the iteration are member fields.

//GEN_INIT should be called on construction.
//GEN_BEGIN should be called at the start of next();
//GEN_YIELD returns a value, and causes the next call to next() to continue from that point.
//GEN_END should be called at the end of next(). It returns a value, and any subsequent calls
//to next() will simply return that same value again.

//NOTE: DO NOT have multiple GEN_YIELDs or GEN_ENDSs on the same line, otherwse __LINE__ will be ambiguous

#define GEN_INIT (genInternalState = 0)
#define GEN_BEGIN switch(genInternalState) { case 0:
#define GEN_YIELD(x) do { genInternalState=__LINE__; return x; case __LINE__:; } while (0)
#define GEN_END(x) genInternalState=__LINE__; case __LINE__: default: break;} return x;

#endif
