
#ifndef BOOSTTHREAD_H_
#define BOOSTTHREAD_H_

#include "../core/global.h"

#ifdef MULTITHREADING
#include <boost/thread.hpp>
#include <boost/atomic.hpp>
#define IS_MULTITHREADING_ENABLED true
#else
#ifdef MULTITHREADING_STD
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
namespace boost
{
  using mutex = std::mutex;
  using thread = std::thread;
  template <typename T>
  using unique_lock = std::unique_lock<T>;
  template <typename T>
  using lock_guard = std::lock_guard<T>;
  template <typename T>
  using condition_variable = std::condition_variable;

  using memory_order = std::memory_order;
  template <typename T>
  using atomic = std::atomic<T>;
}
#define IS_MULTITHREADING_ENABLED true
#else
#define IS_MULTITHREADING_ENABLED false
namespace boost
{
  //FROM boost/thread.hpp------------------------------------------------------------
  class mutex
  { public:
    inline void lock() {};
    inline void unlock() {};
  };
  class thread
  { public:
    inline thread() {};
    inline void join() {};
  };
  template <class T>
  class unique_lock
  { public:
    unique_lock(T t) {(void)t;};
    inline void lock() {};
    inline void unlock() {};
  };
  template <class T>
  class lock_guard
  { public:
    lock_guard(T t) {(void)t;};
  };
  class condition_variable
  { public:
    inline void notify_all() {};
    template <class T>
    inline void wait(unique_lock<T> lock) {(void)lock;};
  };

  //FROM boost/atomic.hpp------------------------------------------------------------
  enum memory_order
  {
      memory_order_relaxed,
      memory_order_acquire,
      memory_order_release,
      memory_order_acq_rel,
      memory_order_seq_cst,
      memory_order_consume,
  };

  template <class T>
  class atomic
  {
    T t;
    public:
    inline atomic() : t() {}
    inline atomic(const atomic<T>& other) : t(other) {}
    inline T& operator=(const T& other) {return (t = other);}
    inline T& load(memory_order m) {(void)m; return t;}
    inline void store(const T& other, memory_order m) {(void)m; t = other;}
  };

}
#endif

#endif

#endif
