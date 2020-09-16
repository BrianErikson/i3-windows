#include <iostream>
#include <i3ipc++/ipc.hpp>
#include <WindowManager.h>
#include <cassert>
#include <cstring>
#include <sstream>
#include <climits>
#include <unistd.h>
#include <fstream>

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
      print_nodes(window.i3node, 1);
    }
  }
}

Json::Value WindowManager::to_json(const WindowContent &window) {
  Json::Value json_children{Json::ValueType::arrayValue};
  for (const auto &child : window.children) {
    json_children.append(this->to_json(child));
  }

  Json::Value json_window{Json::ValueType::objectValue};
  json_window["children"] = json_children;
  json_window["executable"] = window.exe_path;
  json_window["layout"] = layout_to_string(window.i3node->layout);
  json_window["pid"] = window.pid;
  json_window["wm_class"] = window.i3node->window_properties.xclass;

  return json_window;
}

void WindowManager::save_current_layout(const std::string &path) {
  Json::Value root{Json::ValueType::arrayValue};

  auto displays = this->get_display_contents();
  for (const auto &display : displays) {
    Json::Value json_array{Json::ValueType::arrayValue};
    for (const auto &window : display.windows) {
      json_array.append(this->to_json(window));
    }

    Json::Value json_display{Json::ValueType::objectValue};
    json_display["display"] = display.display_name;
    json_display["windows"] = json_array;

    root.append(json_display);
  }

  Json::StyledWriter writer;
  auto json_str = writer.write(root);

  {
    std::ofstream file{path, std::ios_base::out | std::ios_base::trunc};
    // TODO: handle failure modes
    assert(file.is_open());
    file << json_str;
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
    this->x_pid_atom = XInternAtom(this->x_display, "_NET_WM_PID", true);
    assert(this->x_pid_atom);
  }
  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char *prop;
  if (XGetWindowProperty(this->x_display,
                         container->xwindow_id,
                         this->x_pid_atom,
                         0,
                         32,
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
  assert(actual_format == 32);

  long pid;
  memcpy(&pid, reinterpret_cast<char *>(prop), sizeof(long));
  return static_cast<pid_t>(pid);
}

WindowContent WindowManager::get_window(const std::shared_ptr<i3ipc::container_t> &container) {
  std::list<WindowContent> children;

  for (const auto &node : container->nodes) {
    children.emplace_back(this->get_window(node));
  }

  // TODO: handle floating nodes

  pid_t pid = this->get_pid(container);
  return {pid, get_path(pid), children, container};
}

std::string WindowManager::get_path(pid_t pid) {
  std::ostringstream ss;
  ss << "/proc/" << pid << "/exe";

  char path[PATH_MAX+1];
  ssize_t count = readlink(ss.str().c_str(), path, PATH_MAX);
  if (count <= 0) {
    return "";
  }

  path[count] = '\0';
  return path;
}

void WindowManager::load_layout(const std::string& basic_string) {
  // TODO
}
