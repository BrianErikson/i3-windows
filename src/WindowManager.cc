#include <iostream>
#include <i3ipc++/ipc.hpp>
#include <WindowManager.h>
#include <cassert>
#include <cstring>
#include <sstream>
#include <climits>
#include <unistd.h>
#include <fstream>
#include <utils.h>
#include <stdio.h>
#include <I3Container.h>

WindowManager::~WindowManager() {
  if (this->x_display) {
    XFree(this->x_display);
  }

  if (this->x_screen) {
    XFree(this->x_screen);
  }
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
  json_window["args"] = "";
  json_window["layout"] = i3Container::layout_to_string(window.i3node->layout);
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

    Json::Value json_window{Json::ValueType::objectValue};
    json_window["children"] = json_array;
    json_window["executable"] = "";
    json_window["args"] = "";
    json_window["layout"] = i3Container::layout_to_string(i3ipc::ContainerLayout::OUTPUT);
    json_window["wm_class"] = "";

    root.append(json_window);
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
  std::cout << root->name << " " << i3Container::layout_to_string(root->layout) << std::endl;

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

  if (!this->x11_connect()) {
    return false;
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

  char path[PATH_MAX + 1];
  ssize_t count = readlink(ss.str().c_str(), path, PATH_MAX);
  if (count <= 0) {
    return "";
  }

  path[count] = '\0';
  return path;
}

bool WindowManager::load_layout(const std::string &load_path) {
  if (!this->window_listener.connect()) {
    std::cerr << "Unable to connect to i3 dbus daemon" << std::endl;
    return false;
  }

  this->window_listener.set_event_callback([](const i3ipc::window_event_t &ev) {
    // ensure this is thread-safe
    std::cout << "window_event: " << (char)ev.type << std::endl;
    // TODO: propagate event to i3Container tree by wm_class name
  });

  // TODO: better error reporting
  Json::Value root;
  {
    std::ifstream file{load_path};
    if (!utils::file_exists(load_path)) {
      std::cout << "File " << load_path << " does not exist!" << std::endl;
      return false;
    }

    if (!file.is_open()) {
      std::cout << "Unable to open file " << load_path << std::endl;
      return false;
    }

    std::stringstream buf;
    buf << file.rdbuf();

    Json::Reader reader;
    if (!reader.parse(buf, root, false)) {
      std::cerr << "Unable to parse file " << load_path << std::endl;
      return false;
    }
  }

  if (!root.isArray()) {
    std::cerr << "root of json structure is not of type array" << std::endl;
    return false;
  }

  i3Container container_root{this->conn};
  if (!container_root.load(root, nullptr)) {
    std::cerr << "failed to load layout from file " << load_path << std::endl;
    return false;
  }

  // TODO: wait for load to complete before returning? aka blocking call?
  return true;
}

bool WindowManager::x11_connect() {
  if (!this->x_display) {
    this->x_display = XOpenDisplay(nullptr);
    if (!this->x_display) {
      std::cerr << "Could not connect to default DISPLAY" << std::endl;
      return false;
    }

    this->x_screen = XDefaultScreenOfDisplay(this->x_display);
    if (!this->x_screen) {
      std::cerr << "Could not connect to default screen of DISPLAY" << std::endl;
      return false;
    }

    this->x_pid_atom = XInternAtom(this->x_display, "_NET_WM_PID", true);
    if (!this->x_pid_atom) {
      std::cerr << "_NET_WM_PID attribute unavailable. What kind of X11 display is running??"
                << std::endl;
      return false;
    }
  }

  return true;
}

_XRROutputInfo *WindowManager::get_output_info(const std::string &xrandr_display_name) {
  _XRROutputInfo *out{nullptr};

  const auto screen_res = XRRGetScreenResources(this->x_display, this->x_screen->root);
  for (int i = 0; i < screen_res->ncrtc; i++) {
    const auto crtc_info = XRRGetCrtcInfo(this->x_display, screen_res, screen_res->crtcs[i]);
    for (int j = 0; j < crtc_info->noutput; j++) {
      const auto output_info = XRRGetOutputInfo(this->x_display, screen_res,
                                                crtc_info->outputs[j]);
      std::string output_name{output_info->name, static_cast<unsigned long>(output_info->nameLen)};
      if (xrandr_display_name == output_name) {
        out = output_info;
        break;
      }

      XRRFreeOutputInfo(output_info);
    }

    XRRFreeCrtcInfo(crtc_info);
    if (out) {
      break;
    }
  }
  XRRFreeScreenResources(screen_res);

  return out;
}
