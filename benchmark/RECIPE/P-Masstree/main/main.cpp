#include<stdio.h>
#include<string.h>
#include<assert.h>

#include "masstree.h"
#include "WitcherAnnotation.h"

#define VALUE_LEN 8

void run_op(char **op, masstree::masstree* tree, FILE *output_file) {
  int num_or_str;
  uint64_t key;
  char* key_str;
  int range_num;
  char* value;
  uint64_t value_str;
  uint64_t* tmp;
  int ret;
  switch (op[0][0]) {
    case 'i':
      num_or_str = atoi(op[3]);

      if (num_or_str == 0) {
        key = atol(op[1]);
        value = (char*) nvm_alloc(VALUE_LEN);
        memcpy(value, op[2], strlen(op[2])+1);
        masstree::clflush(value, VALUE_LEN, true);

        witcher_tx_begin();
        tree->put(key, value);
        witcher_tx_end();

        fprintf(output_file, "\n");
      } else {
        assert(num_or_str == 1);
        key_str = op[1];
        value_str = atol(op[2]);

        witcher_tx_begin();
        tree->put(key_str, value_str);
        witcher_tx_end();

        fprintf(output_file, "\n");
      }

      break;
    case 'd':
      num_or_str = atoi(op[2]);

      if (num_or_str == 0) {
        key = atol(op[1]);

        witcher_tx_begin();
        tree->del(key);
        witcher_tx_end();

        fprintf(output_file, "\n");
      } else {
        assert(num_or_str == 1);
        key_str = op[1];

        witcher_tx_begin();
        tree->del(key_str);
        witcher_tx_end();

        fprintf(output_file, "\n");
      }

      break;
    case 'g':
      num_or_str = atoi(op[2]);

      if (num_or_str == 0) {
        key = atol(op[1]);

        witcher_tx_begin();
        value = (char*) tree->get(key);
        witcher_tx_end();

        fprintf(output_file, "%s\n", value);
      } else {
        assert(num_or_str == 1);
        key_str = op[1];

        witcher_tx_begin();
        tmp = (uint64_t*) tree->get(key_str);
        witcher_tx_end();

        if (tmp == NULL) {
          fprintf(output_file, "%s\n", NULL);
        } else {
          fprintf(output_file, "%lu\n", *tmp);
        }
      }

      break;
    case 'r':
      num_or_str = atoi(op[3]);

      if (num_or_str == 0) {
        key = atol(op[1]);
        range_num = atoi(op[2]);
        uint64_t results[range_num];
        for (int i = 0; i < range_num; i++) {
          results[i] = 0;
        }

        witcher_tx_begin();
        tree->scan(key, range_num, results);
        witcher_tx_end();

        for (int i = 0; i < range_num; i++) {
          if (results[i] == 0 || atol((char*)results[i]) == 0) {
            break;
          }
          fprintf(output_file, "%s ", (char*)results[i]);
        }
        fprintf(output_file, "\n");
      } else {
        assert(num_or_str == 1);

        key_str = op[1];
        range_num = atoi(op[2]);

        masstree::leafvalue *results[range_num];
        for (int i = 0; i < range_num; i++) {
          results[i] = NULL;
        }

        witcher_tx_begin();
        tree->scan(key_str, range_num, results);
        witcher_tx_end();

        for (int i = 0; i < range_num; i++) {
          if (results[i] == NULL) {
            break;
          }
          fprintf(output_file, "%lu ", results[i]->value);
        }
        fprintf(output_file, "\n");
      }

      break;
  }
}

void read_op_and_run(char *op_file_path,
                     int start_index,
                     int skip_index,
                     char *output_file_path,
                     masstree::masstree* tree) {
  FILE *output_file = fopen(output_file_path, "w");
  FILE *op_file = fopen(op_file_path, "r");
  char line[256];
  int count = 0;
  while (fgets(line, sizeof line, op_file) != NULL) {
    if (count < start_index || count == skip_index) {
      count++;
      continue;
    }

    char *op[4];
    char *p = strtok(line, ";");
    int i = 0;
    while (p != NULL && i < 4) {
      op[i++] = p;
      p = strtok (NULL, ";");
    }

    run_op(op, tree, output_file);

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
  masstree::masstree *tree = masstree::init_P_MASSTREE(pmem_path,
                                                       pmem_size_in_mib,
                                                       layout_name);

  read_op_and_run(op_file_path,
                  start_index,
                  skip_index,
                  output_file_path,
                  tree);

  if (argc == 9) {
    char *memory_layout_path = argv[8];
    witcher_get_memory_layout(memory_layout_path);
  }

  return 0;
}
