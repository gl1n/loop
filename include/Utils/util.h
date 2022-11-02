#pragma once

#include <sys/types.h>
namespace loop {
//获取1970年至今毫秒数
u_int64_t timeSinceEpochMillisecs();

// 禁止拷贝基类。如果子类显示定义了自己的复制函数的话还是能够进行复制的。
class uncopyable {
  // public的话noncopyable能够实例化，private的话子类无法实例化
protected:
  uncopyable() {}
  ~uncopyable() {}

private:
  //禁止拷贝
  uncopyable(const uncopyable &) = delete;
  uncopyable &operator=(const uncopyable &) = delete;
  uncopyable(uncopyable &&) = delete;
  uncopyable &operator=(uncopyable &&) = delete;
};

} // namespace loop