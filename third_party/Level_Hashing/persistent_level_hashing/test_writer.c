#include "level_hashing_printer.h"

int main(int argc, char* argv[]) {
  level_hash *level = level_init("pm.img", 100, "layout", 2);

  uint8_t key[KEY_LEN];
  uint8_t value[VALUE_LEN];

  snprintf(key, KEY_LEN, "%ld", 111);
  snprintf(value, VALUE_LEN, "%ld", 222);
  level_insert(level, key, value);

  snprintf(key, KEY_LEN, "%ld", 333);
  snprintf(value, VALUE_LEN, "%ld", 444);
  level_insert(level, key, value);

  snprintf(key, KEY_LEN, "%ld", 555);
  snprintf(value, VALUE_LEN, "%ld", 666);
  level_insert(level, key, value);

  print_level_hash(level);

  level_destroy(level);

  return 0;
}
