#include <Utils/log.h>
#include <chrono>
#include <memory>
#include <thread>

int main() {
  LOG_DEFAULT;
  // loop::Logger::Instance().add_channel(std::make_shared<loop::FileChannel>());
  loop::Logger::Instance().set_async();
  TraceL << 1;
  DebugL << 2;
  InfoL << 3;
  WarnL << 4;
  ErrorL << 5;
}