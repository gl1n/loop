#include <Utils/semaphore.h>
#include <cstddef>

namespace loop {

Semaphore::Semaphore(size_t initial) { sem_init(&_semaphore, 0, initial); }

Semaphore::~Semaphore() { sem_destroy(&_semaphore); }

void Semaphore::wait() { sem_wait(&_semaphore); }

void Semaphore::post(int n) {
  for (int i = 0; i < n; i++) {
    sem_post(&_semaphore);
  }
}
} // namespace loop