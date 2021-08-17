#include "level_hashing_printer.h"

int main(int argc, char* argv[]) {
  level_hash *level = level_init("pm.img", 100, "layout", 2);
  print_level_hash(level);
  level_destroy(level);
  return 0;
}
