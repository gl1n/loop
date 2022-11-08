#pragma once

namespace loop {

class SockUtil {
public:
  /**
   * 设置读写socket是否堵塞
   * @param fd socket fd
   * @param noblock 是否堵塞
   * @return 0表示成功，-1表示失败
   */
  static int setNoBlocked(int fd, bool noblock = true);
  static int setCloExec(int fd, bool on = true);
};

} // namespace loop