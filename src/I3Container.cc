#include <iostream>
#include <utils.h>
#include "I3Container.h"

std::string i3Container::layout_to_string(i3ipc::ContainerLayout layout) {
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

i3ipc::ContainerLayout i3Container::layout_from_string(const std::string &str) {
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

i3Container::i3Container(const i3ipc::connection &conn) :
conn{conn}
{
}

i3Container::i3Container(const i3Container &other):
conn{other.conn}
{
}

bool i3Container::load(const Json::Value &container, i3Container *parent_container) {
  this->parent = parent_container;

  if (container["layout"].isString()) {
    this->layout = this->layout_from_string(container["layout"].asString());
    if (this->layout == i3ipc::ContainerLayout::UNKNOWN) {
      std::cerr << "Expected valid layout in object:" << std::endl << container.toStyledString()
                << std::endl;
      return false;
    }
  }

  const auto exec_value = container["executable"];
  if ((!exec_value.isString() && !exec_value.empty())
      || (exec_value.empty() && container["children"].empty())) {
    std::cerr << "Expected a valid executable path or children for object:"
              << container.toStyledString() << std::endl;
    return false;
  }

  if (!exec_value.empty()) {
    this->exec_path = exec_value.asString();
    if (container["args"].isString()) {
      this->exec_args = container["args"].asString();
    }

    if (!this->load_as_window()) {
      std::cerr << "Could not recreate window object" << std::endl;
    }
  }
  else {
    if (!this->load_as_layout(container["children"])) {
      std::cerr << "Could not load window layout object" << std::endl;
      return false;
    }
  }

  return true;
}

bool i3Container::load_as_layout(const Json::Value &children_value) {

  // TODO: Handle display json param (OUTPUT layout)
  // TODO: Load this container as a layout first
  // TODO: pass "value" as container to children for appropriate layout generation
  for (const auto &child_value : children_value) {
    i3Container child{this->conn};
    if (!child.load(child_value, this)) {
      std::cerr << "Could not load window object:" << std::endl << child_value.toStyledString()
                << std::endl;
    }
    this->children.emplace_back(child);
  }

  return true;
}

bool i3Container::load_as_window() {
  if (!utils::file_exists(this->exec_path)) {
    std::cerr << "Could not find executable " << this->exec_path <<  std::endl;
    return false;
  }

  if (!this->conn.send_command("exec --no-startup-id " + exec_path + " " + this->exec_args)) {
    return false;
  }

  return true;
}

bool i3Container::load_display_content(const Json::Value &value) {
  // TODO: port and remove
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
}

bool i3Container::load_display(const Json::Value &value) {
  // TODO: port and remove
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

  const auto xrandr_display_name = display_value.asString();
  if (!utils::xrandr_display_connected(xrandr_display_name)) {
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
