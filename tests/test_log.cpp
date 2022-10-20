#include <Utils/log.h>
#include <memory>

int main() {
  LOG_DEFAULT;
  // loop::Logger::Instance().add_channel(std::make_shared<loop::FileChannel>());
  loop::Logger::Instance().set_async();
  int i = 1;
  std::cout << i << std::endl;
  TraceL << 1 << std::endl;
  DebugL << 2 << std::endl;
  InfoL << 3 << std::endl;
  WarnL << 4 << std::endl;
  ErrorL << 5 << std::endl;
}