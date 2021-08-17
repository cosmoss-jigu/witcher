#include<stdio.h>
#include<string.h>
#include<assert.h>

#include "btree.h"
#include "WitcherAnnotation.h"

#define VALUE_LEN 8

#define BUFF_LEN 2048

void run_op(char **op, btree *bt, FILE *output_file) {
  int64_t key;
  int64_t key_min, key_max;
  char* value;
  switch (op[0][0]) {
    case 'i':
      key = atol(op[1]);
      value = (char*) nvm_alloc(VALUE_LEN);
      memcpy(value, op[2], strlen(op[2])+1);
      clflush(value, VALUE_LEN);

      witcher_tx_begin();
      bt->btree_insert(key, value);
      witcher_tx_end();

      fprintf(output_file, "\n");

      break;
    case 'd':
      key = atol(op[1]);

      witcher_tx_begin();
      bt->btree_delete(key);
      witcher_tx_end();

      fprintf(output_file, "\n");
      break;
    case 'g':
      key = atol(op[1]);

      witcher_tx_begin();
      value = bt->btree_search(key);
      witcher_tx_end();

      fprintf(output_file, "%s\n", value);
      break;
    case 'r':
      key_min = atol(op[1]);
      key_max = atol(op[2]);

      unsigned long buff[BUFF_LEN];
      for (int i = 0; i < BUFF_LEN; i++) {
        buff[i] = 0;
      }

      witcher_tx_begin();
      bt->btree_search_range(key_min, key_max, buff);
      witcher_tx_end();

      for (int i = 0; i < BUFF_LEN; i++) {
        if (buff[i] == 0) {
          break;
        }
        fprintf(output_file, "%s ", ((char*)buff[i]));
      }
      fprintf(output_file, "\n");
      break;
  }
}

void read_op_and_run(char *op_file_path,
                     int start_index,
                     int skip_index,
                     char *output_file_path,
                     btree *bt) {
  FILE *output_file = fopen(output_file_path, "w");
  FILE *op_file = fopen(op_file_path, "r");
  char line[256];
  int count = 0;
  while (fgets(line, sizeof line, op_file) != NULL) {
    if (count < start_index || count == skip_index) {
      count++;
      continue;
    }

    char *op[3];
    char *p = strtok(line, ";");
    int i = 0;
    while (p != NULL && i < 3) {
      op[i++] = p;
      p = strtok (NULL, ";");
    }

    run_op(op, bt, output_file);
    //bt->printAll();

    count++;
  }
  fclose(op_file);
  fclose(output_file);
}

int main(int argc, char *argv[]) {
  assert(argc== 8 || argc == 9);

  char *pmem_path = argv[1];
  size_t pmem_size_in_mib = atoi(argv[2]);
  char *layout_name = argv[3];

  char *op_file_path = argv[4];
  int start_index = atoi(argv[5]);
  int skip_index = atoi(argv[6]);

  char *output_file_path = argv[7];
  btree *bt = init_FastFair(pmem_path, pmem_size_in_mib, layout_name);

  read_op_and_run(op_file_path,
                  start_index,
                  skip_index,
                  output_file_path,
                  bt);

  if (argc == 9) {
    char *memory_layout_path = argv[8];
    witcher_get_memory_layout(memory_layout_path);
  }

  return 0;
}
