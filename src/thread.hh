#pragma once

#include <functional>
#include <thread>

namespace vptyp {
class Thread {
 public:
  Thread() = default;
  Thread(Thread&& other);
  Thread& operator=(Thread&& other);
  Thread(Thread& other) = delete;
  Thread& operator=(Thread&) = delete;
  virtual ~Thread();

  void thread_run(std::function<void(std::stop_token stoken)> func);
  void thread_stop();

 protected:
  std::jthread thr;
  std::atomic<bool> running{false};
};
}  // namespace vptyp