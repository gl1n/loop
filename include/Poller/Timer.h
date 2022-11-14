#pragma once

#include "Poller/EventPoller.h"
#include <functional>
#include <memory>
namespace loop {

class Timer {
public:
  using Ptr = std::shared_ptr<Timer>;

  Timer(float second, const std::function<bool()> &cb,
        const EventPoller::Ptr pooler, bool continueWhenException = true);

  ~Timer();

private:
  DelayTask::Ptr _tag;
  //绑定某个固定的pooler
  EventPoller::Ptr _pooler;
};

} // namespace loop