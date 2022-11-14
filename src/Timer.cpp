#pragma once
#include "Poller/Timer.h"
#include "Poller/EventPoller.h"
#include "Utils/log.h"

namespace loop {
Timer::Timer(float second, const std::function<bool()> &cb,
             const EventPoller::Ptr pooler, bool continueWhenException) {
  _pooler = pooler;
  if (!_pooler) {
    _pooler = EventPollerPool::Instance().getPoller();
  }

  //为什么不能对cb引用捕获?
  //因为cb在本函数执行结束后就被销毁！这里没有移动语义，一定要注意对象的生命周期！
  _tag = _pooler->doDelayTask(1000 * second, [=]() -> u_int64_t {
    try {
      if (cb()) {
        //重复任务
        return static_cast<u_int64_t>(second * 1000);
      } else {
        return static_cast<u_int64_t>(0);
      }
    } catch (std::exception &ex) {
      ErrorL << "执行定时器任务捕获到异常:" << ex.what();
      return continueWhenException ? static_cast<u_int64_t>(second * 1000)
                                   : static_cast<u_int64_t>(0);
    }
  });
}

Timer::~Timer() { _tag->cancel(); }
} // namespace loop