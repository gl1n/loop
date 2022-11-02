#include "Thread/TaskExecutor.h"
#include "Thread/semaphore.h"
#include "Utils/onceToken.h"
#include "Utils/util.h"
#include <mutex>
#include <sys/types.h>

namespace loop {

/**********************ThreadLoadCounter***********************/
ThreadLoadCounter::ThreadLoadCounter(u_int64_t max_size, u_int64_t max_usec) {
  _last_sleep_time = _last_wake_up_time = timeSinceEpochMillisecs();
  _max_size = max_size;
  _max_usec = max_usec;
}

void ThreadLoadCounter::recordSleep() {
  std::lock_guard<std::mutex> lg(_mtx);
  _sleeping = true;
  auto current_time = timeSinceEpochMillisecs();
  auto run_time = current_time - _last_wake_up_time;
  _last_sleep_time = current_time;
  _time_list.emplace_back(
      run_time,
      false); //现在由wakeup状态变成sleep状态，记录过去这一段时间的wake up时长
  if (_time_list.size() > _max_size) {
    _time_list.pop_front();
  }
}

void ThreadLoadCounter::recordWakeUp() {
  std::lock_guard<std::mutex> lg(_mtx);
  _sleeping = false;
  auto current_time = timeSinceEpochMillisecs();
  auto sleep_time = current_time - _last_sleep_time;
  _last_wake_up_time = current_time;
  _time_list.emplace_back(sleep_time, true);
  if (_time_list.size() > _max_size) {
    _time_list.pop_front();
  }
}

int ThreadLoadCounter::load() {
  std::lock_guard<std::mutex> lg(_mtx);
  u_int64_t totalSleepTime = 0, totalRunTime = 0;
  //计算_time_list内记录的时间
  _time_list.for_each([&totalSleepTime, &totalRunTime](const TimeRecord &r) {
    if (r._sleep) {
      totalSleepTime += r._time;
    } else {
      totalRunTime += r._time;
    }
  });
  //计算当前未被记录成TimeRecord的时间
  if (_sleeping) {
    totalSleepTime += (timeSinceEpochMillisecs() - _last_sleep_time);
  } else {
    totalRunTime += (timeSinceEpochMillisecs() - _last_wake_up_time);
  }

  u_int64_t totalTime = totalRunTime + totalSleepTime;
  while ((!_time_list.empty()) &&
         (totalTime > _max_usec || _time_list.size() > _max_size)) {
    auto r = _time_list.front();
    if (r._sleep) {
      totalSleepTime -= r._time;
    } else {
      totalRunTime -= r._time;
    }
    totalTime -= r._time;
    _time_list.pop_front();
  }

  if (totalTime == 0) {
    return 0;
  }

  return (int)(totalRunTime * 100 / totalTime);
}

/**********************TaskExecutorInterface***********************/

Task::Ptr TaskExecutorInterface::async_first(TaskIn task, bool may_sync) {
  return async(std::move(task), may_sync);
}

void TaskExecutorInterface::sync(const TaskIn &task) {
  Semaphore sem;
  auto ret = async([&]() {
    onceToken token(nullptr, [&]() { sem.post(); });
    task();
  });
  if (ret && *ret) {
    sem.wait();
  }
}

void TaskExecutorInterface::sync_first(const TaskIn &task) {
  Semaphore sem;
  auto ret = async_first([&]() {
    onceToken token(nullptr, [&]() { sem.post(); });
    task();
  });
  if (ret && *ret) {
    sem.wait();
  }
};

/**********************TaskExecutor***********************/
TaskExecutor::TaskExecutor(u_int64_t max_size, u_int64_t max_usec)
    : ThreadLoadCounter(max_size, max_usec) {}

} // namespace loop