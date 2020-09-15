#include <unistd.h>
#include <argp.h>
#include <iostream>
#include <config.h>

const char *argp_program_version = "i3-windows " PROJECT_VERSION;
const char *argp_program_bug_address = "briankerikson@protonmail.com";
static char doc[] = PROJECT_DESCRIPTION;
static char args_doc[] = "[FILENAME]...";

int main(int argc, char **argv) {


  if (geteuid() != 0) {
    std::cout << "Not running as superuser! Some properties may not be able to be retrieved."
              << std::endl;
  }
  return 0;
}