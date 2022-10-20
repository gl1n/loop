#include <Utils/log.h>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

int main() {
  LOG_DEFAULT;
  loop::Logger::Instance().add_channel(std::make_shared<loop::FileChannel>());
  loop::Logger::Instance().set_async();
  TraceL << "第一条log";
  DebugL << 12.345;
  InfoL << "sleep for 2 seconds";
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  std::string warn("This is a warning");
  WarnL << warn;
  ErrorL << '!';
}