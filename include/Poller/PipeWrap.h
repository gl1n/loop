#pragma once

namespace loop {
class PipeWrap {
public:
  PipeWrap();
  ~PipeWrap();

  //向管道写入数据
  int write(const void *buf, int n);

  //从管道中读取数据
  int read(void *buf, int n);

  int get_read_fd() const { return _pipe_fd[0]; }

  int get_write_fd() const { return _pipe_fd[1]; }

private:
  //关闭管道
  void close_pipe();

private:
  int _pipe_fd[2] = {-1, -1};
};
} // namespace loop