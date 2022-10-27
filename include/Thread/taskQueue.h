#pragma once

#include <Thread/semaphore.h>
#include <Utils/list.h>
#include <cstddef>
#include <mutex>
#include <utility>

namespace loop {
//这个类用于同步地管理任务队列
template <typename T> class TaskQueue {
public:
  // 将任务加入队尾
  template <typename C> void push_task_back(C &&task_func) {
    {
      std::lock_guard<decltype(_mutex)> lg(_mutex);
      _queue.emplace_back(std::forward<C>(task_func));
    }
    _sem.post();
  }
  // 将任务加入队头
  template <typename C> void push_task_first(C &&task_func) {
    {
      std::lock_guard<decltype(_mutex)> lg(_mutex);
      _queue.emplace_front(std::forward<C>(task_func));
    }
    _sem.post();
  }
  //清空任务队列
  void push_exit(size_t n) { _sem.post(n); }

  //从队列获取一个任务，由执行线程执行
  bool get_task(T &tsk) {
    _sem.wait();
    std::lock_guard<decltype(_mutex)> lg(_mutex);
    if (_queue.empty()) {
      return false;
    }
    tsk = std::move(_queue.front()); //移动复制
    _queue.pop_front();
    return true;
  }

  size_t size() const {
    std::lock_guard<decltype(_mutex)> lg(
        _mutex); // const函数但是需要改变_mutex的状态，所以将_mutex定义为mutable
    return _queue.size();
  }

private:
  mutable std::mutex _mutex; // mutable的作用是打破const函数的限制
  loop::List<T> _queue;
  loop::Semaphore _sem;
};
} // namespace loop