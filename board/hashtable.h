/*
 * hashtable.h
 * Author: davidwu
 */

#ifndef HASHTABLE_H_
#define HASHTABLE_H_

#include "../board/btypes.h"

class FastExistsHashTable
{
  public:
  FastExistsHashTable(int exp);
  ~FastExistsHashTable();

  bool lookup(hash_t hash);
  void record(hash_t hash);
  void clear();

  private:
  int size;
  hash_t mask;
  int clearOffset;
  hash_t* keys;
};


#endif /* HASHTABLE_H_ */
