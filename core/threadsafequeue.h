/*
 * threadsafequeue.h
 * Author: davidwu
 */

#ifndef THREADSAFEQUEUE_H
#define THREADSAFEQUEUE_H

#include "../core/global.h"
#include "../core/boostthread.h"

template<typename T>
class ThreadSafeQueue
{
  vector<T> elts;
  size_t headIdx;
  std::mutex mutex;
  std::condition_variable emptyCondVar;

  public:
  inline ThreadSafeQueue()
  :elts(),headIdx(0),mutex(),emptyCondVar()
  {}
  inline ~ThreadSafeQueue()
  {}

  inline size_t size()
  {
    std::lock_guard<std::mutex> lock(mutex);
    DEBUGASSERT(elts.size() >= headIdx);
    return elts.size() - headIdx;
  }


  inline void push(T elt)
  {
    std::lock_guard<std::mutex> lock(mutex);
    elts.push_back(elt);
    if(elts.size() == headIdx + 1)
      emptyCondVar.notify_all();
  }

  inline bool tryPop(T& buf)
  {
    std::lock_guard<std::mutex> lock(mutex);
    if(elts.size() <= headIdx)
      return false;
    buf = unsynchronizedPop();
    return true;
  }

  inline T waitPop()
  {
    std::unique_lock<std::mutex> lock(mutex);
    while(elts.size() <= headIdx)
      emptyCondVar.wait(lock);
    return unsynchronizedPop();
  }

  private:
  inline T unsynchronizedPop()
  {
    T elt = elts[headIdx];
    headIdx++;
    size_t eltsSize = elts.size();
    if(headIdx > eltsSize / 2)
    {
      DEBUGASSERT(eltsSize >= headIdx);
      size_t len = eltsSize - headIdx;
      for(size_t i = 0; i<len; i++)
        elts[i] = elts[i+headIdx];
      elts.resize(len);
      headIdx = 0;
    }
    return elt;
  }

};

#endif
