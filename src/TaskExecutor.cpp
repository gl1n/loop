#include "Thread/TaskExecutor.h"
#include "Thread/semaphore.h"
#include "Utils/TimeTicker.h"
#include "Utils/util.h"
#include <memory>
#include <mutex>
#include <sys/types.h>
#include <vector>

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

/**********************TaskExecutor***********************/
TaskExecutor::TaskExecutor(u_int64_t max_size, u_int64_t max_usec)
    : ThreadLoadCounter(max_size, max_usec) {}

TaskExecutor::Ptr TaskExecutorGetterImp::getExecutor() {
  //从上一次取到的执行器的位置开始找
  auto executor_pos = _last_lightest_executor_pos;
  if (executor_pos >= _executors.size()) {
    executor_pos = 0;
  }

  TaskExecutor::Ptr min_load_executor = _executors[executor_pos];
  auto min_load = min_load_executor->load();

  for (int i = 0; i < _executors.size(); i++, executor_pos++) {
    //相当于取模
    if (executor_pos >= _executors.size()) {
      executor_pos = 0;
    }

    auto executor = _executors[executor_pos];
    auto load = executor->load();

    if (load < min_load) {
      min_load_executor = executor;
      min_load = load;
    }

    //负载不可能比0小，所以找到负载为0的执行器直接退出
    if (min_load == 0) {
      break;
    }
  }
  _last_lightest_executor_pos = executor_pos;
  return min_load_executor;
}

std::vector<int> TaskExecutorGetterImp::getExecutorLoad() {
  std::vector<int> ret;
  for (auto const &exec : _executors) {
    ret.push_back(exec->load());
  }
  return ret;
}

void TaskExecutorGetterImp::getExecutorDelay(
    const std::function<void(const std::vector<int> &)> &callback) {
  int totalCount = _executors.size();
  std::shared_ptr<std::atomic_int> completed =
      std::make_shared<std::atomic_int>(0);
  std::shared_ptr<std::vector<int>> delayVec =
      std::make_shared<std::vector<int>>(totalCount);
  int index = 0;
  for (auto &exec : _executors) {
    std::shared_ptr<Ticker> ticker = std::make_shared<Ticker>();
    exec->async(
        [delayVec, index, ticker, completed, totalCount, &callback]() {
          (*delayVec)[index] = ticker->sinceLast();
          (*completed)++;
          if ((*completed) == totalCount) {
            callback(*delayVec);
          }
        },
        false); //不能直接执行，必须打入队列
    index++;
  }
}

} // namespace loop