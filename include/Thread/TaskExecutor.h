#pragma once

#include "Utils/list.h"
#include "Utils/util.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <sys/types.h>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

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

/**
 * 任务执行器
 */
class TaskExecutor : public ThreadLoadCounter {
public:
  using Task = std::function<void()>;
  using Ptr = std::shared_ptr<TaskExecutor>;

  TaskExecutor(u_int64_t max_size = 32, u_int64_t max_usec = 2 * 1000 * 1000);
  virtual ~TaskExecutor() = default;

  /**
   * 异步执行任务
   * @param task 任务
   * @param may_sync 是否允许同步执行该任务
   * @return 任务是否添加成功
   */
  virtual bool async(Task &&task, bool may_sync = true) = 0;

  /**
   * 最高优先级方式异步执行任务
   * @param task 任务
   * @param may_sync 是否允许同步执行该任务
   * @return 任务是否添加成功
   */
  virtual bool async_first(Task &&task, bool may_sync = true) {
    return async(std::move(task));
  }

  /**
   * 同步执行任务
   * @param task 任务
   */
  virtual bool sync(Task &&task) = 0;

  /**
   * 最高优先级方式同步执行任务
   * @param task 任务
   */
  virtual bool sync_first(Task &&task) { return sync(std::move(task)); }
};

class TaskExecutorGetter {
public:
  using Ptr = std::shared_ptr<TaskExecutorGetter>;
  virtual ~TaskExecutorGetter() = default;

  /**
   * 获取任务执行器
   * @return 任务执行器
   */
  virtual TaskExecutor::Ptr getExecutor() = 0;
};

class TaskExecutorGetterImp : public TaskExecutorGetter {
public:
  TaskExecutorGetterImp() = default;
  ~TaskExecutorGetterImp() = default;

  /**
   * 根据线程负载情况，获取最空闲的任务执行器
   * @return 任务执行器
   */
  TaskExecutor::Ptr getExecutor() override;

  /**
   * 获取所有线程的负载率
   * @return 所有线程的负载率
   */
  std::vector<int> getExecutorLoad();

  /**
   * 测算完成当前所有执行器内的任务所需时间
   * @param callback 完成测算之后进行的回调
   */
  void getExecutorDelay(
      const std::function<void(const std::vector<int> &)> &callback);

  template <typename FUNC> void for_each(FUNC &&func) {
    for (auto &ex : _executors) {
      func(ex);
    }
  }

protected:
  template <typename FUNC>
  //统一创建执行器组
  void createExecutor(FUNC &&func,
                      int executor_num = std::thread::hardware_concurrency()) {
    for (int i = 0; i < executor_num; i++) {
      _executors.emplace_back(func());
    }
  }

private:
  std::vector<TaskExecutor::Ptr> _executors;
  int _last_lightest_executor_pos = 0;
};

} // namespace loop