#include "thread.hh"

namespace vptyp {

Thread::Thread(Thread&& other) {
  thr = std::move(other.thr);
  running.store(other.running.load());
  other.running.store(false);
}
Thread& Thread::operator=(Thread&& other) {
  if (this != &other) {
    thr = std::move(other.thr);
    running.store(other.running.load());
    other.running.store(false);
  }
  return *this;
}
Thread::~Thread() { thread_stop(); }

void Thread::thread_run(std::function<void(std::stop_token stoken)> func) {
  thread_stop();

  thr = std::jthread([this, func](std::stop_token stoken) {
    running.store(true);
    func(stoken);
    running.store(false);
  });
}

void Thread::thread_stop() {
  if (running.load()) {
    thr.request_stop();
    thr.join();
  }
}

}  // namespace vptyp