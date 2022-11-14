#include "Poller/Timer.h"
#include "Thread/semaphore.h"
#include "Utils/TimeTicker.h"
#include "Utils/log.h"
#include <csignal>
#include <exception>
#include <memory>
#include <stdexcept>

using namespace loop;
int main() {
  LOG_DEFAULT;
  Logger::Instance().set_async();

  Ticker ticker0;
  Timer::Ptr timer0 = std::make_shared<Timer>(
      0.5,
      [&]() {
        TraceL << "timer0重复:" << ticker0.sinceLast();
        ticker0.resetTime();
        return true;
      },
      nullptr);

  Timer::Ptr timer1 = std::make_shared<Timer>(
      1.0,
      []() {
        DebugL << "timer1不再重复";
        return false;
      },
      nullptr);

  Ticker ticker2;
  Timer::Ptr timer2 = std::make_shared<Timer>(
      2.0,
      [&]() -> bool {
        InfoL << "timer2, 测试中任务抛出异常" << ticker2.sinceLast();
        ticker2.resetTime();
        throw std::runtime_error("timer2,测试任务中抛出异常");
      },
      nullptr);

  static Semaphore sem;
  signal(SIGINT, [](int) { sem.post(); });
  sem.wait();
}