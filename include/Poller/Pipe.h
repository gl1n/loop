#pragma once

#include "Poller/EventPoller.h"
#include <functional>
#include <memory>

namespace loop {
class Pipe {
public:
  Pipe(const std::function<void(int size, const char *buf)> &onRead = nullptr,
       const EventPoller::Ptr &poller = nullptr);

  ~Pipe();

  void send(const char *send, int size = 0);

private:
  std::shared_ptr<PipeWrap> _pipe;
  EventPoller::Ptr _poller;

  // Pipe(const onRead &cb = nullptr, )
};
} // namespace loop