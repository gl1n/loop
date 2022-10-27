#include <Thread/semaphore.h>
#include <Utils/log.h>
#include <chrono>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

loop::Semaphore g_sem;
std::mutex g_mtx;

int main() {
  LOG_DEFAULT;

  std::queue<int> q;

  std::vector<std::thread> threads;

  int num = 4;
  for (int i = 0; i < num; i++) {
    threads.emplace_back([&q, i]() {
      while (true) {
        {
          std::lock_guard<std::mutex> lg(g_mtx);
          q.push(i + 1);
          InfoL << "Produce item: " << q.back();
        }
        g_sem.post();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
      }
    });
  }
  for (int i = 0; i < num; i++) {
    threads.emplace_back([&q]() {
      while (true) {
        g_sem.wait();
        {
          std::lock_guard<std::mutex> lg(g_mtx);
          InfoL << "Consume item: " << q.front();
          q.pop();
        }
      }
    });
  }
  //必须join或者detach，因为main函数退出时所有std::thread对象都会被析构，如果这些对象是joinable，则会调用terminate
  for (auto &th : threads) {
    th.join();
  }
}