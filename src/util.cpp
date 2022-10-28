#include "Utils/util.h"
#include <bits/chrono.h>
#include <chrono>
#include <sys/types.h>

namespace loop {

u_int64_t timeSinceEpochMillisecs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

} // namespace loop