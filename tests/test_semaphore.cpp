#include "Thread/semaphore.h"
#include "Thread/threadgroup.h"
#include "Utils/TimeTicker.h"
#include "Utils/log.h"
#include <atomic>
#include <cstddef>

constexpr int MAX_TASK_SIZE = 1000 * 10000;

loop::Semaphore g_sem;
std::atomic_llong g_produced(0);
std::atomic_llong g_consumed(0);

//生产者线程
void onProduce() {
  while (true) {
    if (g_produced >= MAX_TASK_SIZE) {
      break;
    }
    g_produced++;
    g_sem.post();
  }
}

//消费者线程
void onConsume() {
  while (true) {
    g_sem.wait();
    g_consumed++;
    if (g_consumed > g_produced) {
      ErrorL << g_consumed << " > " << g_produced;
    }
  }
}

int main() {
  LOG_DEFAULT;

  loop::Ticker ticker;
  loop::ThreadGroup producers;
  for (size_t i = 0; i < std::thread::hardware_concurrency(); ++i) {
    producers.create_thread([]() { onProduce(); });
  }

  loop::ThreadGroup consumers;
  for (size_t i = 0; i < 4; i++) {
    consumers.create_thread([]() { onConsume(); });
  }

  producers.join_all();

  DebugL << "生产者线程退出，耗时：" << ticker.sinceLast() << "ms,"
         << "生产任务数：" << g_produced << "，消费任务数" << g_consumed;

  int i = 5;
  while (i--) {
    DebugL << "程序退出倒计时：" << i << "消费任务数：" << g_consumed;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  WarnL << "强制关闭线程，可能会发生coredump";
  return 0;
}