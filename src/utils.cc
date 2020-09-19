#include <sys/stat.h>
#include "utils.h"

bool utils::file_exists(const std::string &path) {
  struct stat buffer{};
  return stat(path.c_str(), &buffer) == 0;
}
