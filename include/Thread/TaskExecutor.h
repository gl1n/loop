#pragma once

#include "Utils/list.h"
#include "Utils/util.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <sys/types.h>
#include <type_traits>
#include <utility>

namespace loop {
/*
 * cpu负载计算器
 */
class ThreadLoadCounter {
public:
  /**
   * 构造函数
   * @param max_size 统计样本数量
   * @param max_usec 统计时间窗口，亦即最近max_usec的cpu负载率
   */
  ThreadLoadCounter(u_int64_t max_size, u_int64_t max_usec);
  ~ThreadLoadCounter() = default;

  /**
   * 记录线程进入休眠
   */
  void recordSleep();

  /**
   * 记录线程被唤醒
   */
  void recordWakeUp();

  /**
   * 返回当前cpu使用率，范围是0 ~ 100
   * @return 当前线程cpu使用率
   */
  int load();

private:
  class TimeRecord {
  public:
    TimeRecord(u_int64_t tm, bool slp) : _time(tm), _sleep(slp) {}

  public:
    u_int64_t _time;
    bool _sleep;
  };

private:
  bool _sleeping = true;
  u_int64_t _last_sleep_time;
  u_int64_t _last_wake_up_time;
  u_int64_t _max_size;
  u_int64_t _max_usec;
  std::mutex _mtx;
  List<TimeRecord> _time_list;
};

class TaskCancelable : public uncopyable {
public:
  TaskCancelable() = default;
  virtual ~TaskCancelable() = default;
  virtual void cancel() = 0;
};

//必须写成先声明再特化，这样才能推导出R和ArgTypes
template <typename R, typename... ArgTypes> class TaskCancelableImp;

template <typename R, typename... ArgTypes>
class TaskCancelableImp<R(ArgTypes...)> : public TaskCancelable {
public:
  using Ptr = std::shared_ptr<TaskCancelableImp>;
  using func_type = std::function<R(ArgTypes...)>;

  ~TaskCancelableImp() = default;

  template <typename FUNC> TaskCancelableImp(FUNC &&task) {
    _strongTask = std::make_shared<func_type>(std::forward<FUNC>(task));
    _weakTask = _strongTask;
  }

  void cancel() override {
    _strongTask = nullptr; //等价于reset()
  }

  operator bool() { return _strongTask && *_strongTask; }

  void operator=(std::nullptr_t) { _strongTask = nullptr; }

  R operator()(ArgTypes... args) const {
    auto strongTask = _weakTask.lock();
    if (strongTask && *strongTask) {
      return (*strongTask)(std::forward<ArgTypes>(args)...);
    }
    return defaultValue<R>();
  }

  //如果T是void，不返回任何值
  template <typename T>
  static typename std::enable_if<std::is_void<T>::value, void>::type
  defaultValue() {}

  template <typename T>
  static typename std::enable_if<std::is_pointer<T>::value, T>::type
  defaultValue() {
    return nullptr;
  }

  template <typename T>
  static typename std::enable_if<std::is_integral<T>::value, T>::type
  defaultValue() {
    return 0;
  }

protected:
  std::weak_ptr<func_type> _weakTask;
  std::shared_ptr<func_type> _strongTask;
};

using TaskIn = std::function<void()>;
using Task = TaskCancelableImp<void()>;

class TaskExecutorInterface {
public:
  TaskExecutorInterface() = default;
  virtual ~TaskExecutorInterface() = default;

  /**
   * 异步执行任务
   * @param task 任务
   * @param may_sync 是否允许同步执行该任务
   * @return 任务是否添加成功
   */
  virtual Task::Ptr async(TaskIn task, bool may_sync = true) = 0;

  /**
   * 最高优先级方式异步执行任务
   * @param task 任务
   * @param may_sync 是否允许同步执行该任务
   * @return 任务是否添加成功
   */
  virtual Task::Ptr async_first(TaskIn task, bool may_sync = true);

  /**
   * 同步执行任务
   * @param task 任务
   */
  void sync(const TaskIn &task);

  /**
   * 最高优先级方式同步执行任务
   * @param task 任务
   */
  void sync_first(const TaskIn &task);
};

/**
 * 任务执行器
 */
class TaskExecutor : public ThreadLoadCounter, public TaskExecutorInterface {
public:
  using Ptr = std::shared_ptr<TaskExecutor>;

  /**
   * 构造函数
   * @param max_size cpu负载统计样本数
   * @param max_usec cpu负载统计时间窗口大小
   */
  TaskExecutor(u_int64_t max_size = 32, u_int64_t max_usec = 2 * 1000 * 1000);
  ~TaskExecutor() = default;
};

} // namespace loop