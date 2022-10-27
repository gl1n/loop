#pragma once

#include <memory>
#include <stdexcept>
#include <thread>
#include <unordered_map>

namespace loop {
class ThreadGroup {
public:
  ThreadGroup(ThreadGroup const &) = delete;

  ThreadGroup &operator=(ThreadGroup const &) = delete;

  ThreadGroup() {}

  ~ThreadGroup() { _threads.clear(); }
  //当前线程是否在ThreadGroup中

  bool is_this_thread_in() {
    auto this_thread_id = std::this_thread::get_id();
    //这个判断其实是多余的，但是有概率不用查map，可能能够提升性能？
    if (this_thread_id == _last_created_thread_id) {
      return true;
    }
    return _threads.find(this_thread_id) != _threads.end();
  }

  // th线程是否在ThreadGroup中
  bool is_thread_in(std::thread *th) {
    // 判断是否为空，防止底下th->get_id()操作失败
    if (!th) {
      return false;
    }
    return _threads.find(th->get_id()) != _threads.end();
  }

  //往ThreadGroup中添加线程
  template <typename F> std::thread *create_thread(F &&func) {
    auto thread_new = std::make_shared<std::thread>(func);
    _last_created_thread_id = thread_new->get_id();
    _threads[_last_created_thread_id] = thread_new;
    return thread_new.get();
  }

  //从map中删除线程
  void remove_thread(std::thread *th) {
    auto it = _threads.find(th->get_id());
    if (it != _threads.end()) {
      _threads.erase(it);
    }
  }

  //当前线程等待ThreadGroup中的线程退出
  void join_all() {
    if (is_this_thread_in()) {
      throw std::runtime_error("ThreadGroup: trying joining itself");
    }
    for (auto &it : _threads) {
      if (it.second->joinable()) {
        it.second->join();
      }
    }
    _threads.clear();
  }

private:
  using THREAD_ID = std::thread::id;
  using THREAD_PTR = std::shared_ptr<std::thread>;
  THREAD_ID _last_created_thread_id;
  std::unordered_map<THREAD_ID, THREAD_PTR> _threads;
};
} // namespace loop