#pragma once

#include <cstddef>
#include <functional>
namespace loop {
class onceToken {
public:
  using task = std::function<void(void)>;

  template <typename FUNC>
  onceToken(const FUNC &onConStructed, task onDestructed = nullptr) {
    onConStructed();
    _onDestructed = std::move(onDestructed);
  }

  onceToken(std::nullptr_t, task onDestructed = nullptr) {
    _onDestructed = std::move(onDestructed);
  }

  ~onceToken() {
    if (_onDestructed) {
      _onDestructed();
    }
  }

private:
  onceToken() = delete;
  onceToken(const onceToken &) = delete;
  onceToken(onceToken &&) = delete;
  onceToken &operator=(const onceToken &) = delete;
  onceToken &operator=(onceToken &&) = delete;

private:
  task _onDestructed;
};
} // namespace loop