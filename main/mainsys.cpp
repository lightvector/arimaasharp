/*
 * mainsys.cpp
 * Author: davidwu
 */

#include "../core/cachelinesize.h"
#include "../core/global.h"
#include "../main/main.h"

#include "../core/boostthread.h"

using namespace std;
int MainFuncs::getCacheLineSize(int argc, const char* const *argv)
{
  (void)argc;
  (void)argv;
  size_t size = CacheLineSize::getCacheLineSize();
  cout << "Cache line size: " << size << " bytes" << endl;
  cout << "Globals is currently compiled to buffer with " << CACHE_LINE_BUFFER_SIZE << " bytes" << endl;
  cout << "sizeof(std::mutex) = " << sizeof(std::mutex) << endl;
  cout << "sizeof(std::condition_variable) = " << sizeof(std::condition_variable) << endl;
  cout << "sizeof(std::thread) = " << sizeof(std::thread) << endl;
  return EXIT_SUCCESS;
}







