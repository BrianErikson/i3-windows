#include <gtest/gtest.h>
#include <WindowManager.h>

TEST(unittests, init) {
  WindowManager wm;
  wm.print_tree();
  wm.save_current_layout("./layout.json");
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}