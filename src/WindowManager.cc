#include <iostream>
#include <i3ipc++/ipc.hpp>
#include <WindowManager.h>
#include <assert.h>

std::string layout_to_string(i3ipc::ContainerLayout layout) {
  switch (layout) {
    case i3ipc::ContainerLayout::UNKNOWN: return "UNKNOWN";
    case i3ipc::ContainerLayout::SPLIT_H: return "SPLIT_H";
    case i3ipc::ContainerLayout::SPLIT_V: return "SPLIT_V";
    case i3ipc::ContainerLayout::STACKED: return "STACKED";
    case i3ipc::ContainerLayout::TABBED: return "TABBED";
    case i3ipc::ContainerLayout::DOCKAREA: return "DOCKAREA";
    case i3ipc::ContainerLayout::OUTPUT: return "OUTPUT";
  }

  return "";
}

void WindowManager::print_tree() {
  for (const auto &display_container : this->get_display_contents()) {
    std::cout << "Display: " << display_container.display_name << std::endl;

    for (const auto &window : display_container.windows) {
      std::cout << window.pid << std::endl;
      print_nodes(window.i3node, 1);
    }
  }
}

void WindowManager::print_nodes(const std::shared_ptr<i3ipc::container_t> &root, int cur_depth) {
  for (int i = 0; i < cur_depth; i++) {
    std::cout << "\t";
  }
  std::cout << root->name << " " << layout_to_string(root->layout) << std::endl;

  for (const auto &node : root->nodes) {
    print_nodes(node, cur_depth + 1);
  }
}

std::vector<DisplayContent> WindowManager::get_display_contents() {
  auto root_containers = conn.get_tree()->nodes;

  std::vector<DisplayContent> out;
  for (const auto &container : root_containers) {
    if (container->name == "__i3") {
      continue;
    }

    auto nodes = container->nodes;
    for (const auto &node : nodes) {
      if (node->name != "content") {
        continue;
      }

      std::list<WindowContent> windows;
      for (const auto &subnode : node->nodes) {
        windows.emplace_back(this->get_window(subnode));
      }

      out.emplace_back(DisplayContent{container->name, windows});
    }
  }

  return out;
}

pid_t WindowManager::get_pid(const std::shared_ptr<i3ipc::container_t> &container) {
  if (!container->xwindow_id) {
    return 0;
  }

  if (!this->x_display) {
    this->x_display = XOpenDisplay(nullptr);
    assert(this->x_display);
    this->x_screen = XDefaultScreenOfDisplay(this->x_display);
    assert(this->x_screen);
    this->x_pid_atom = XInternAtom(this->x_display, "__NET_WM_PID", true);
    assert(this->x_pid_atom);
  }

  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after, nbytes;
  unsigned char *prop;
  if (XGetWindowProperty(this->x_display,
                         container->xwindow_id,
                         this->x_pid_atom,
                         0,
                         sizeof(pid_t),
                         false,
                         AnyPropertyType,
                         &actual_type,
                         &actual_format,
                         &nitems,
                         &bytes_after,
                         &prop) != Success) {
    return 0;
  }
  if (!actual_format) {
    return 0;
  }

  return static_cast<pid_t>(atoi(reinterpret_cast<char *>(prop)));
}

WindowContent WindowManager::get_window(const std::shared_ptr<i3ipc::container_t> &container) {
  std::list<WindowContent> children;

  for (const auto &node : container->nodes) {
    children.emplace_back(this->get_window(node));
  }

  return {this->get_pid(container), children, container};
}
