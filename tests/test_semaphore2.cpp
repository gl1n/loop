#include "Thread/semaphore.h"
#include "Thread/threadgroup.h"
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
    g_produced++;
    g_sem.post();
    if (g_produced >= MAX_TASK_SIZE) {
      break;
    }
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

  loop::ThreadGroup producers;
  for (size_t i = 0; i < std::thread::hardware_concurrency(); ++i) {
    producers.create_thread([]() { onProduce(); });
  }

  loop::ThreadGroup consumers;
  for (size_t i = 0; i < 4; i++) {
    consumers.create_thread([]() { onConsume(); });
  }

  producers.join_all();
  consumers.join_all();
}