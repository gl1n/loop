#include "Thread/ThreadPool.h"
#include "Thread/semaphore.h"
#include "Utils/TimeTicker.h"
#include "Utils/log.h"
#include "Utils/onceToken.h"
#include <chrono>
#include <thread>
#include <vector>

using namespace std;
using namespace loop;
int main() {
  LOG_DEFAULT;
  Logger::Instance().set_async();

  ThreadPool pool(thread::hardware_concurrency());
  //每个任务执行三秒
  int task_time = 3;
  //每个线程平均执行4个任务
  int task_count = thread::hardware_concurrency() * 4;

  Semaphore sem;
  vector<int> vec(task_count);
  Ticker ticker;

  {
    auto token = make_shared<onceToken>(nullptr, [&sem]() { sem.post(); });

    for (int i = 0; i < task_count; i++) {
      pool.async([token, i, &vec, task_time]() {
        this_thread::sleep_for(chrono::seconds(task_time));
        InfoL << "task " << i << " done!";
        vec[i] = i * i;
      });
    }
  }

  // token对象以及它的复制品全部析构之后才会执行sem.post();
  sem.wait();
  InfoL << "All tasks done! " << ticker.sinceLast()
        << " milliseconds was taken.";

  // 打印任务执行结果
  for (auto const &v : vec) {
    InfoL << v;
  }
  return 0;
}