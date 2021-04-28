/*
 * hashtable.cpp
 * Author: davidwu
 */

#include "../core/global.h"
#include "../board/btypes.h"
#include "../board/hashtable.h"

FastExistsHashTable::FastExistsHashTable(int exp)
{
  if(exp <= 0 || exp >= 31)
    Global::fatalError("FastExistsHashTable: exp must be between 0 and 31");
  size = 1 << exp;
  mask = (hash_t)(size-1);
  keys = new hash_t[size];
  clearOffset = 0;
  clear();
}

FastExistsHashTable::~FastExistsHashTable()
{
  delete[] keys;
}

void FastExistsHashTable::clear()
{
  if(clearOffset <= 0 || clearOffset >= size - 2)
  {
    //A hash of value i+1 can only get stored in an index of value i if clearOffset == size - 1
    //so that adding it to the indended index causes it to wrap around.
    for(int i = 0; i<size; i++)
      keys[i] = (hash_t)i+1;
    clearOffset = 1;
  }
  else
  {
    clearOffset++;
  }

}

bool FastExistsHashTable::lookup(hash_t hash)
{
  int hashIndex = (int)((hash + clearOffset) & mask);
  if(keys[hashIndex] == hash)
    return true;
  int hashIndex2 = (int)(((hash >> 32) + clearOffset) & mask);
  if(keys[hashIndex2] == ((hash >> 32) | (hash << 32)))
    return true;

  return false;
}

void FastExistsHashTable::record(hash_t hash)
{
  int hashIndex = (int)((hash + clearOffset) & mask);
  int hashIndex2 = (int)(((hash >> 32) + clearOffset) & mask);
  keys[hashIndex] = hash;
  //Swap bytes so that the lower order bits of the stored value are always exactly
  //the hashslot, offset by clearOffset.
  keys[hashIndex2] = ((hash >> 32) | (hash << 32));
}



