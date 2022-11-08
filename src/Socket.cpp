#include "Network/Socket.h"
#include "Utils/log.h"
#include "fcntl.h"
#include <fcntl.h>

namespace loop {
int SockUtil::setNoBlocked(int fd, bool noblock) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    TraceL << "设置O_NONBLOCK失败!";
    return -1;
  }
  int ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if (ret == -1) {
    TraceL << "设置 O_NONBLOCK 失败!";
    return -1;
  }
  return ret;
}

int SockUtil::setCloExec(int fd, bool on) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    TraceL << "设置 FD_CLOEXEC 失败!";
    return -1;
  }
  if (on) {
    flags |= FD_CLOEXEC;
  } else {
    flags &= ~FD_CLOEXEC;
  }

  int ret = fcntl(fd, F_SETFL, flags);
  if (ret == -1) {
    TraceL << "设置 FD_CLOEXEC 失败!";
    return -1;
  }
  return ret;
}

} // namespace loop