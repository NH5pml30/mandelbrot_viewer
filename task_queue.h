#pragma once

#include <condition_variable>

#include "intrusive_list.h"

/* (Multi-thread safe) queue for prioritized tasks */
template<class T, typename Tag, size_t max_priority>
class task_queue_ex
{
public:
  struct store_type : public T
  {
    bool is_tasked = false;
  };

  // pre: enclosing mutex is locked
  bool empty() const;
  // pre: enclosing mutex is locked, lg is its unique_lock
  store_type &pop(std::unique_lock<std::mutex> &lg);
  // pre: enclosing mutex is locked
  void push(store_type &p);
  // pre: enclosing mutex is locked
  bool erase(store_type &p);

private:
  intrusive::list<store_type, Tag> queue[max_priority + 1];
  std::condition_variable cv;
  mutable size_t cached_non_empty;
};

template<class T, typename Tag, size_t max_priority>
bool task_queue_ex<T, Tag, max_priority>::empty() const
{
  for (size_t i = max_priority + 1; i > 0; i--)
    if (!queue[i - 1].empty())
    {
      cached_non_empty = i - 1;
      return false;
    }
  return true;
}

// pre: enclosing mutex is locked, lg is its unique_lock
template<class T, typename Tag, size_t max_priority>
auto task_queue_ex<T, Tag, max_priority>::pop(std::unique_lock<std::mutex> &lg) -> store_type &
{
  cv.wait(lg, [this] { return !empty(); });

  store_type &result = queue[cached_non_empty].front();
  queue[cached_non_empty].pop_front();
  result.is_tasked = false;
  return result;
}

// pre: enclosing mutex is locked
template<class T, typename Tag, size_t max_priority>
void task_queue_ex<T, Tag, max_priority>::push(store_type &p)
{
  queue[p.priority()].push_back(p);
  p.is_tasked = true;
  cv.notify_one();
}

// pre: enclosing mutex is locked
template<class T, typename Tag, size_t max_priority>
bool task_queue_ex<T, Tag, max_priority>::erase(store_type &p)
{
  if (p.is_tasked)
  {
    size_t prior = p.priority();
    queue[prior].erase(queue[prior].iterator_from_el(p));
    p.is_tasked = false;
    return true;
  }
  return false;
}

/* (Multi-thread safe) counter waiting for target */
struct WAITING_COUNTER
{
  std::atomic<size_t> val = 0;
  size_t target = 0;
  std::condition_variable cv;

  // pre: enclosing mutex is locked, lg is its unique_lock
  void wait(std::unique_lock<std::mutex> &lg, size_t target)
  {
    this->target = target;
    val = 0;
    cv.wait(lg, [this, target] { return val.load() >= target; });
  }

  // pre: enclosing mutex is locked, lg is its unique_lock (unlocks)
  void increment(std::unique_lock<std::mutex> &lg)
  {
    val.fetch_add(1);
    bool need_notify = val >= target;
    lg.unlock();
    if (need_notify)
      cv.notify_one();
  }
};
