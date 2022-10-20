#include <Utils/log.h>
#include <chrono>
#include <memory>
#include <thread>

int main() {
  LOG_DEFAULT;
  // loop::Logger::Instance().add_channel(std::make_shared<loop::FileChannel>());
  loop::Logger::Instance().set_async();
  TraceL << 1 << std::endl;
  DebugL << 2;
  InfoL << 3 << std::endl;
  WarnL << 4 << std::endl;
  ErrorL << 5 << std::endl;
  std::this_thread::sleep_for(std::chrono::milliseconds(10000));
}