#include "Poller/EventPoller.h"
#include "Utils/TimeTicker.h"
#include "Utils/log.h"
#include <cstdlib>
#include <signal.h>
#include <sstream>
#include <unistd.h>

using namespace std;
using namespace loop;

int main() {
  static bool exit_flag = false;
  signal(SIGINT, [](int) { exit_flag = true; });

  LOG_DEFAULT;
  Ticker ticker;
  while (!exit_flag) {
    if (ticker.sinceLast() > 1000) {
      auto vec = EventPollerPool::Instance().getExecutorLoad();

      stringstream ss;
      for (auto load : vec) {
        ss << load << '-';
      }
      DebugL << "cpu负载: " << ss.str();

      EventPollerPool::Instance().getExecutorDelay(
          [](const std::vector<int> &vec) {
            stringstream ss;
            for (auto delay : vec) {
              ss << delay << "-";
            }
            DebugL << "cpu任务执行延时:" << ss.str();
          });
      ticker.resetTime();
    }

    EventPollerPool::Instance().getExecutor()->async([]() {
      auto usec = rand() % 4000;
      usleep(usec);
    });

    usleep(2000);
  }

  return 0;
}