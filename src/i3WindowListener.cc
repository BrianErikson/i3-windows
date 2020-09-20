#include <i3ipc++/ipc.hpp>
#include <iostream>
#include "i3WindowListener.h"

i3WindowListener::~i3WindowListener() {
  if (this->thread_run && this->thread->joinable()) {
    this->thread_run = false;
    this->thread->join();
  }
}

void i3WindowListener::set_event_callback(const std::function<void(const i3ipc::window_event_t&)>& callback) {
  std::lock_guard<std::mutex> lock{this->conn_mut};
  this->conn.signal_window_event.connect(callback);
}

bool i3WindowListener::connect() {
  if (this->thread_run) {
    return true;
  }

  std::promise<bool> state_promise;
  std::future<bool> future_state = state_promise.get_future();
  this->thread = std::make_unique<std::thread>(&i3WindowListener::run, this, std::move(state_promise));

  future_state.wait();
  return future_state.get();
}

void i3WindowListener::run(std::promise<bool> state_promise) {
  this->thread_run = true;

  bool success = false;
  {
    std::lock_guard<std::mutex> lock{this->conn_mut};
    success = conn.subscribe(i3ipc::ET_WINDOW);
  }

  state_promise.set_value(success);

  if (!success) {
    this->thread_run = false;
    return;
  }

  while (this->thread_run) {
    std::lock_guard<std::mutex> lock{this->conn_mut};
    conn.handle_event();
  }
}
