#include <unistd.h>
#include <argp.h>
#include <iostream>
#include <fstream>
#include <config.h>
#include <WindowManager.h>
#include <sys/stat.h>
#include "utils.h"

const char *argp_program_version = "i3-windows " PROJECT_VERSION;
const char *argp_program_bug_address = "briankerikson@gmail.com";
static char doc[] = PROJECT_DESCRIPTION;
static char args_doc[] = "";
static struct argp_option options[] = {
    { "save", 's', "FILENAME", OPTION_ARG_OPTIONAL,
      "Save the current window layout (takes precedence over load)"},
    { "load", 'l', "FILENAME", OPTION_ARG_OPTIONAL, "Load a window layout"},
    { 0 }
};

struct Arguments {
  enum Mode {SAVE, LOAD};
  bool enabled{false};
  std::string path{};
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
  auto *arguments = static_cast<Arguments *>(state->input);
  switch (key) {
    case 's':
      arguments[static_cast<int>(Arguments::Mode::SAVE)].enabled = true;
      arguments[static_cast<int>(Arguments::Mode::SAVE)].path = arg;
      break;
    case 'l':
      arguments[static_cast<int>(Arguments::Mode::LOAD)].enabled = true;
      arguments[static_cast<int>(Arguments::Mode::LOAD)].path = arg;
      break;
    case ARGP_KEY_ARG:
      return 0;
    default:
      return ARGP_ERR_UNKNOWN;
  }

  return 0;
}

static struct argp argp = {options, parse_opt, args_doc, doc, nullptr, nullptr, nullptr};


int main(int argc, char **argv) {
  if (!argc) {
    return 0;
  }

  Arguments arguments[2];
  arguments[0] = {};
  arguments[1] = {};

  argp_parse(&argp, argc, argv, 0, nullptr, &arguments);

  if (geteuid() != 0) {
    std::cout << "Not running as superuser; some properties may not be able to be retrieved."
              << std::endl;
  }

  bool save_enabled = arguments[static_cast<int>(Arguments::SAVE)].enabled;
  bool load_enabled = arguments[static_cast<int>(Arguments::LOAD)].enabled;
  std::string save_path = arguments[static_cast<int>(Arguments::SAVE)].path;
  std::string load_path = arguments[static_cast<int>(Arguments::SAVE)].path;

  if (load_enabled && !utils::file_exists(load_path)) {
    std::cout << "Unable to open file: " << load_path << std::endl;
    return 1;
  }

  WindowManager wm;

  if (save_enabled) {
    wm.save_current_layout(save_path);
  }

  if (load_enabled) {
    wm.load_layout(load_path);
  }

  return 0;
}