#include "Poller/EventPoller.h"
#include "Thread/semaphore.h"
#include "Utils/TimeTicker.h"
#include "Utils/log.h"
#include "Utils/onceToken.h"
#include <csignal>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <sys/types.h>
#include <unistd.h>

using namespace std;
using namespace loop;

int main() {
  LOG_DEFAULT;
  Logger::Instance().set_async();

  Ticker t0;
  int next_delay0 = 50;
  std::shared_ptr<onceToken> token0 =
      std::make_shared<onceToken>(nullptr, []() {
        TraceL << "task 0 被取消, 可以立即触发释放lambda表达式捕获的变量!";
      });
  auto tag0 = EventPollerPool::Instance().getPoller()->doDelayTask(
      next_delay0, [&, token0]() { //注意token0是指针
        TraceL << "task 0(固定延时重复任务), 预期休眠时间: " << next_delay0
               << "实际休眠时间: " << t0.sinceLast();
        t0.resetTime();
        return next_delay0;
      });
  token0 = nullptr; //这条语句减少use_count

  Ticker t1;
  int next_delay1 = 50;
  auto tag1 =
      EventPollerPool::Instance().getPoller()->doDelayTask(next_delay1, [&]() {
        DebugL << "task 1(可变延时重复任务), 预期休眠时间 : " << next_delay1
               << "实际休眠时间" << t1.sinceLast();
        t1.resetTime();
        next_delay1 += 1;
        return next_delay1;
      });

  Ticker t2;
  int next_delay2 = 3000;
  auto tag2 =
      EventPollerPool::Instance().getPoller()->doDelayTask(next_delay2, [&]() {
        InfoL << "task 2(单次延时任务), 预期休眠时间 : " << next_delay2
              << "实际休眠时间" << t2.sinceLast();
        return 0;
      });

  Ticker t3;
  int next_delay3 = 50;
  auto tag3 = EventPollerPool::Instance().getPoller()->doDelayTask(
      next_delay3, [&]() -> u_int64_t {
        throw std::runtime_error(
            "task 3(测试延时中抛出异常, 将导致不再继续该延时任务)");
      });

  sleep(2);
  tag0->cancel();
  tag1->cancel();

  WarnL << "取消task 0、1";

  static Semaphore sem;
  signal(SIGINT, [](int) { sem.post(); });
  sem.wait();
}