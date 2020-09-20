#pragma once
#include <thread>
#include <future>
#include <stdatomic.h>

class i3WindowListener {
 public:
  ~i3WindowListener();
  bool connect();

  // callback will be executed in a separate thread. Ensure proper thread safety in callback.
  void set_event_callback(const std::function<void(const i3ipc::window_event_t&)>& callback);

 private:
  void run(std::promise<bool> state_promise);

  std::unique_ptr<std::thread> thread{nullptr};

  atomic_bool thread_run;
  std::mutex conn_mut{};
  i3ipc::connection conn{};
};


