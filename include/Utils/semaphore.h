#pragma once
#include <cstddef>
#include <semaphore.h>

namespace loop {
class Semaphore {
public:
  explicit Semaphore(size_t initial = 0);
  ~Semaphore();
  void wait();
  void post(int n = 1);

private:
  sem_t _semaphore;
};
} // namespace loop