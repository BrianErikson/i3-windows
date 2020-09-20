#pragma once
#include <vector>
#include <i3ipc++/ipc.hpp>
#include <i3/ipc.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <jsoncpp/json/json.h>
#include "i3WindowListener.h"

struct WindowContent {
  const pid_t pid;
  const std::string exe_path;
  const std::list<WindowContent> children;
  const std::shared_ptr<i3ipc::container_t> i3node;
};

struct DisplayContent {
  const std::string display_name;
  const std::list<WindowContent> windows;
};

class WindowManager {
 public:
  ~WindowManager();
  void print_tree();
  void save_current_layout(const std::string &path);
  bool load_layout(const std::string& load_path);

 private:
  static std::string get_path(pid_t pid);

  std::vector<DisplayContent> get_display_contents();
  void print_nodes(const std::shared_ptr<i3ipc::container_t>& root, int cur_depth);
  WindowContent get_window(const std::shared_ptr<i3ipc::container_t>& container);
  pid_t get_pid(const std::shared_ptr<i3ipc::container_t>& container);
  Json::Value to_json(const WindowContent &window);
  bool x11_connect();

  // Ownership of returned ptr goes to caller
  _XRROutputInfo* get_output_info(const std::string &xrandr_display_name);

  i3WindowListener window_listener{};
  _XDisplay *x_display{nullptr};
  Screen *x_screen{nullptr};
  Atom x_pid_atom{0};
  i3ipc::connection conn;
};
