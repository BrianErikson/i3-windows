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

i3ipc::ContainerLayout layout_from_string(const std::string &str) {
  if (str == "SPLIT_H") {
    return i3ipc::ContainerLayout::SPLIT_H;
  }
  if (str == "SPLIT_V") {
    return i3ipc::ContainerLayout::SPLIT_V;
  }
  if (str == "STACKED") {
    return i3ipc::ContainerLayout::STACKED;
  }
  if (str == "TABBED") {
    return i3ipc::ContainerLayout::TABBED;
  }
  if (str == "DOCKAREA") {
    return i3ipc::ContainerLayout::DOCKAREA;
  }
  if (str == "OUTPUT") {
    return i3ipc::ContainerLayout::OUTPUT;
  }

  return i3ipc::ContainerLayout::UNKNOWN;
}

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

bool WindowManager::load_window_layout(const Json::Value &value) {
  if (value["layout"].empty()) {
    std::cerr << "Cannot load an empty window layout for object:" << value.toStyledString()
              << std::endl;
    return false;
  }
  if (!value["executable"].empty()) {
    std::cerr << "Expected a window layout but found executable in object:"
              << value.toStyledString() << std::endl;
    return false;
  }
  if (value["children"].empty()) {
    std::cerr << "Window object must either contain children or an executable path. Object:"
              << std::endl << value.toStyledString() << std::endl;
    return false;
  }

  // TODO: pass "value" as container to children for appropriate layout generation

  for (const auto &child : value["children"]) {
    if (!this->load_window(child, value)) {
      std::cerr << "Could not load window object:" << value.toStyledString() << std::endl;
    }
  }

  return true;
}

bool WindowManager::load_window(const Json::Value &value, const Json::Value &parent) {
  if (parent.empty()) {
    // TODO: Does a window always have a parent layout?
    return false;
  }

  const auto exec_value = value["executable"];
  if (exec_value.empty()) {
    std::cerr << "Expected an executable value for object:" << std::endl << value.toStyledString()
              << std::endl;
    return false;
  }
  else if (!exec_value.isString()) {
    std::cerr << "Invalid executable member field in object:" << std::endl << value.toStyledString()
              << std::endl;
    return false;
  }

  const std::string exec_path{exec_value.asString()};
  if (!utils::file_exists(exec_path)) {
    std::cerr << "Could not find executable " << exec_path << " from object " << std::endl
              << value.toStyledString() << std::endl;
    return false;
  }

  std::string args{""};
  if (value["args"].isString()) {
    args = value["args"].asString();
  }

  this->window_listener.set_event_callback([](const i3ipc::workspace_event_t &ev) {
    // ensure this is thread-safe
    std::cout << "workspace_event: " << (char)ev.type << std::endl;
  });

  if (!this->conn.send_command("exec --no-startup-id " + exec_path + " " + args)) {
    return false;
  }

  // TODO: wait for process to idle (maybe start other processes in parallel?)
  // TODO: Move identified i3 window attached to process to the location specified by json


  return true;
}

bool WindowManager::load_display_content(const Json::Value &value) {
  if (!value.isObject()) {
    std::cerr << "Expected object in json entry: " << std::endl << value.toStyledString()
              << std::endl;
    return false;
  }
  if (value["layout"].isInt() && static_cast<i3ipc::ContainerLayout>(value["layout"].asInt())
      == i3ipc::ContainerLayout::OUTPUT) {
    std::cerr << "Nested displays are not supported. Object:" << std::endl << value.toStyledString()
             << std::endl;
    return false;
  }

  /*
  json_window["executable"] = window.exe_path;
  json_window["args"] = "";
  json_window["layout"] = layout_to_string(window.i3node->layout);
  json_window["pid"] = window.pid;
  json_window["wm_class"] = window.i3node->window_properties.xclass;
   */

  if (!this->load_window_layout(value)) {
    if (!this->load_window(value, Json::Value{})) {
      std::cerr << "Unidentified object:" << std::endl << value.toStyledString() << std::endl;
      return false;
    }
  }

}

bool WindowManager::load_display(const Json::Value &value) {
  if (!value.isObject()) {
    std::cerr << "Expected object in json entry: " << std::endl << value.toStyledString()
              << std::endl;
    return false;
  }

  const auto display_value = value["display"];
  if (display_value.empty() || !display_value.isString()) {
    std::cerr << "Invalid display member field in object:" << std::endl << value.toStyledString()
              << std::endl;
    return false;
  }

  if (!this->x11_connect()) {
    return false;
  }

  const auto xrandr_display_name = display_value.asString();
  if (!this->xrandr_display_connected(xrandr_display_name)) {
    return false;
  }

  const auto windows_value = value["windows"];
  if (windows_value.empty() || !windows_value.isArray()) {
    std::cerr << "Invalid windows member field in object:" << std::endl << value.toStyledString()
              << std::endl;
    return false;
  }
  for (const auto &window_value : windows_value) {
    if (!this->load_display_content(window_value)) {
      std::cerr << "Could not load window value " << std::endl << window_value.toStyledString()
        << std::endl;
    }
  }

  return true;
}

bool WindowManager::load_layout(const std::string &load_path) {
  if (!this->window_listener.connect()) {
    std::cerr << "Unable to connect to i3 dbus daemon" << std::endl;
    return false;
  }

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

  for (const auto &display : root) {
    this->load_display(display);
  }
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

bool WindowManager::xrandr_display_connected(const std::string &xrandr_display_name) {
  const auto output = this->get_output_info(xrandr_display_name);
  if (!output) {
    std::cerr << "Could not find connected output XRandR display " << xrandr_display_name
              << std::endl;
    return false;
  }

  XRRFreeOutputInfo(output);
  return true;
}
