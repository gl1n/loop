#pragma once

#include "Thread/TaskExecutor.h"
#include "Thread/semaphore.h"
#include "Thread/taskQueue.h"
#include "Thread/threadgroup.h"
#include "Utils/log.h"
#include <bits/types/struct_sched_param.h>
#include <cstddef>
#include <exception>
#include <memory>
#include <pthread.h>
#include <sched.h>
#include <thread>

namespace loop {

class ThreadPool : public TaskExecutor {
public:
  enum Priority {
    PRIORITY_LOWEST = 0,
    PRIORITY_LOW,
    PRIORITY_NORMAL,
    PRIORITY_HIGH,
    PRIORITY_HIGHEST
  };

  ThreadPool(int num = 1, Priority priority = PRIORITY_HIGHEST,
             bool auto_run = true)
      : _thread_num(num), _priority(priority) {
    //默认线程池直接启动
    if (auto_run) {
      start();
    }
  }

  ~ThreadPool() {
    //等待任务队列处理完毕并收到空信号
    shutdown();
    //等待线程退出
    wait();
  }

  bool async(Task &&task, bool may_sync = true) override {
    //如果允许同步执行而且当前线程就在线程池里，就直接执行任务
    if (may_sync && _thread_group.is_this_thread_in()) {
      task();
    } else {
      _queue.push_task_back(std::move(task));
    }
    return true;
  }

  bool async_first(Task &&task, bool may_sync = true) override {
    if (may_sync && _thread_group.is_this_thread_in()) {
      task();
    } else {
      _queue.push_task_first(std::move(task));
    }

    return true;
  }

  bool sync(Task &&task) override {
    Semaphore sem;
    bool flag = async([&]() {
      task();
      sem.post();
    });
    if (flag) {
      sem.wait();
    }
    return flag;
  }

  bool sync_first(Task &&task) override {
    Semaphore sem;
    bool flag = async_first([&]() {
      task();
      sem.post();
    });
    if (flag) {
      sem.wait();
    }
    return flag;
  }

  //设置线程优先级，默认设置本线程
  static bool setPriority(Priority priority = PRIORITY_NORMAL,
                          std::thread::native_handle_type thread_id = 0) {
    static int Min = sched_get_priority_min(SCHED_OTHER);
    if (Min == -1) {
      return false;
    }
    static int Max = sched_get_priority_max(SCHED_OTHER);
    if (Max == -1) {
      return false;
    }
    static int Priorities[] = {Min, Min + (Max - Min) / 4,
                               Min + (Max - Min) / 2, Min + 3 * (Max - Min) / 4,
                               Max};
    // 默认设置本线程的Priority
    if (thread_id == 0) {
      thread_id = pthread_self();
    }
    struct sched_param params;
    params.sched_priority = Priorities[priority];
    //设置线程优先级，成功则返回true;
    return pthread_setschedparam(thread_id, SCHED_OTHER, &params) == 0;
  }

  //启动线程池
  void start() {
    if (_thread_num <= 0) {
      return;
    }
    size_t total = _thread_num - _thread_group.size();
    for (size_t i = 0; i < total; i++) {
      _thread_group.create_thread([this]() { run(); });
    }
  }

private:
  //每个线程的职责是从_queue中取出任务并执行
  void run() {
    ThreadPool::setPriority(_priority);
    TaskExecutor::Task task;

    while (true) {
      recordSleep();
      if (!_queue.get_task(task)) {
        // get_task会阻塞等待_queue，如果返回false说明收到空信号
        break;
      }
      recordWakeUp();
      //执行task
      try {
        task();
        task = nullptr;
      } catch (std::exception &ex) {
        ErrorL << "ThreadPool执行任务捕获到异常:" << ex.what();
      }
    }
  }

  //等待线程退出
  void wait() { _thread_group.join_all(); }

  //给_queue放信号
  void shutdown() { _queue.push_exit(_thread_num); }

private:
  std::size_t _thread_num;
  TaskQueue<TaskExecutor::Task> _queue;
  ThreadGroup _thread_group;
  Priority _priority;
  //保持日志器可用
  // Logger::Ptr _logger;
};

} // namespace loop