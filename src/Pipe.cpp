#include "Poller/Pipe.h"
#include "Poller/EventPoller.h"
#include <memory>
#include <sys/ioctl.h>

namespace loop {
Pipe::Pipe(const std::function<void(int size, const char *buf)> &onRead,
           const EventPoller::Ptr &poller) {
  _poller = poller;
  if (!_poller) {
    _poller = EventPollerPool::Instance().getPoller();
  }
  _pipe = std::make_shared<PipeWrap>();

  auto pipeCopy = _pipe;
  _poller->addEvent(_pipe->get_read_fd(), PollEvent::EpollRead,
                    [onRead, pipeCopy](int event) {
                      int nread = 1024;
                      ioctl(pipeCopy->get_read_fd(), FIONREAD, &nread); //
                      char buf[nread + 1]; //这里会不会有问题?
                      nread = pipeCopy->read(buf, nread);
                      if (onRead) {
                        onRead(nread, buf);
                      }
                    });
}

Pipe::~Pipe() {
  if (_pipe) {
    auto pipeCopy = _pipe;
    _poller->delEvent(_pipe->get_read_fd());
  }
}

void Pipe::send(const char *buf, int size) { _pipe->write(buf, size); }
} // namespace loop