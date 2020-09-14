#pragma once
#include <vector>
#include <i3ipc++/ipc.hpp>
#include <i3/ipc.h>

struct DisplayContent {
  const std::string display_name;
  const std::list<std::shared_ptr<i3ipc::container_t>> content;
};

class WindowManager {
 public:
  void print_tree();
 private:
  std::vector<DisplayContent> get_display_contents();
  void print_nodes(const std::shared_ptr<i3ipc::container_t>& root, int cur_depth);
  i3ipc::connection conn;
};
