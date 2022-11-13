#include "Poller/PipeWrap.h"
#include "Network/Socket.h"
#include "unistd.h"
#include <asm-generic/errno-base.h>
#include <cstring>
#include <errno.h>
#include <stdexcept>
#include <string>

namespace loop {
PipeWrap::PipeWrap() {
  if (pipe(_pipe_fd) == -1) {
    std::string err_msg = "create posix pipe failed:";
    throw std::runtime_error(err_msg + std::strerror(errno));
  }
  SockUtil::setNoBlocked(_pipe_fd[0], true);
  SockUtil::setNoBlocked(_pipe_fd[1], false);
  SockUtil::setCloExec(_pipe_fd[0]);
  SockUtil::setCloExec(_pipe_fd[1]);
}
PipeWrap::~PipeWrap() { close_pipe(); }

int PipeWrap::write(const void *buf, int n) {
  int ret;
  do {
    //::使用全局的write函数，即系统api，与本函数区分开
    ret = ::write(_pipe_fd[1], buf, n);
  } while (ret == -1 && errno == EINTR);

  return ret;
}

int PipeWrap::read(void *buf, int n) {
  int ret;
  //这个过程是非堵塞的
  do {
    ret = ::read(_pipe_fd[0], buf, n);
  } while (ret == -1 && errno == EINTR);

  return ret;
}

void PipeWrap::close_pipe() {
  if (_pipe_fd[0] != -1) {
    close(_pipe_fd[0]);
    _pipe_fd[0] = -1;
  }
  if (_pipe_fd[1] != -1) {
    close(_pipe_fd[1]);
    _pipe_fd[1] = -1;
  }
}
} // namespace loop