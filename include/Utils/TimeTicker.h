#pragma once

#include "Utils/log.h"
#include "Utils/util.h"
#include <sys/types.h>
namespace loop {
class Ticker {
public:
  Ticker(u_int64_t expected_time = 0,
         LogEventCapture lec = LogEventCapture(Logger::Instance(),
                                               LogLevel::Warn, __FILE__, "",
                                               __LINE__),
         bool print_log = false)
      : _expected_time(expected_time), _lec(std::move(lec)) {
    if (!print_log) {
      _lec.clear();
    }
    _create_time = timeSinceEpochMillisecs();
    _begin_time = _create_time;
  }

  ~Ticker() {
    if (sinceCreated() > _expected_time) {
      _lec << "take time: " << sinceCreated()
           << "ms, thread might be overloaded"; //超出预计时间
    } else {
      _lec.clear(); //未超时，不需要打印
    }
  }

  u_int64_t sinceCreated() { return timeSinceEpochMillisecs() - _create_time; }

  u_int64_t sinceLast() { return timeSinceEpochMillisecs() - _begin_time; }

  //重新计时
  void resetTime() { _begin_time = timeSinceEpochMillisecs(); }

private:
  u_int64_t _expected_time;
  u_int64_t _begin_time;
  u_int64_t _create_time;
  LogEventCapture _lec;
};
} // namespace loop