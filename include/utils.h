#pragma once
#include <string>

namespace utils
{

bool file_exists(const std::string &path);
bool xrandr_display_connected(const std::string &xrandr_display_name);

}


