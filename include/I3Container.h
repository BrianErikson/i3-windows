#pragma once
#include <optional>
#include <i3ipc++/ipc.hpp>
#include <jsoncpp/json/json.h>
#include "i3WindowListener.h"

class i3Container {
 public:
  explicit i3Container(const i3ipc::connection &conn);
  i3Container(const i3Container &other);

  bool load(const Json::Value &container, i3Container *parent_container);
  static std::string layout_to_string(i3ipc::ContainerLayout layout);
  static i3ipc::ContainerLayout layout_from_string(const std::string &layout);

 private:
  bool load_display(const Json::Value &value);
  bool load_display_content(const Json::Value &value);
  bool load_as_window();
  bool load_as_layout(const Json::Value &children_value);

  const i3ipc::connection &conn;
  i3ipc::ContainerLayout layout{i3ipc::ContainerLayout::UNKNOWN};
  std::vector<std::string> loading_classes{};
  std::string display{};
  std::string exec_path{};
  std::string exec_args{};
  std::string wm_class{};
  std::vector<i3Container> children{};
  i3Container *parent{nullptr};
};
