#include <sys/stat.h>
#include <iostream>
#include "utils.h"

bool utils::file_exists(const std::string &path) {
  struct stat buffer{};
  return stat(path.c_str(), &buffer) == 0;
}

bool xrandr_display_connected(const std::string &xrandr_display_name) {
  const auto output = this->get_output_info(xrandr_display_name);
  if (!output) {
    std::cerr << "Could not find connected output XRandR display " << xrandr_display_name
              << std::endl;
    return false;
  }

  XRRFreeOutputInfo(output);
  return true;
}
