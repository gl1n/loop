#include "Poller/Pipe.h"
#include "Thread/semaphore.h"
#include "Utils/log.h"
#include <csignal>
#include <string>
#include <unistd.h>

using namespace loop;
int main() {
  LOG_DEFAULT;

  auto parent_pid = getpid();
  InfoL << "parent pid: " << parent_pid;

  Pipe pipe([](int size, const char *buf) {
    std::string str(buf);
    InfoL << "Thread: " << getpid() << " recv: " << str.substr(0, size);
  });

  auto pid = fork();
  /**
   * https://stackoverflow.com/questions/39890363/what-happens-when-a-thread-forks
   * A forked child is an exact duplicate of its parent, yet only the thread
   * that called fork() in the parent, still exists in the child and is the new
   * main thread of that child until you call exec().
   */

  /**
   * 只有调用fork的线程会出现在子线程中，所以EventPoller的线程就不会在子线程中出现
   */

  if (pid == 0) //子进程
  {
    for (int i = 0; i < 10; i++) {
      sleep(1);
      std::stringstream ss;
      ss << "message " << i << " from subprocess: " << getpid();
      DebugL << "子进程发送: " << ss.str();
      pipe.send(ss.str().data(), ss.str().size());
    }
    DebugL << "子进程退出";
  } else { //父进程
    //父进程设置退出信号处理函数
    static Semaphore sem;
    signal(SIGINT, [](int) { sem.post(); });
    sem.wait();

    InfoL << "父进程退出";
  }
  return 0;
}