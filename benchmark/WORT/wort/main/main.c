#include<stdio.h>
#include<string.h>
#include<assert.h>

#include "wort.h"
#include "pmdk.h"
#include "WitcherAnnotation.h"

#define VALUE_LEN 8

void run_op(char **op, art_tree* tree, FILE *output_file) {
  uint64_t key;
  char* value;
  char* ret;
  switch (op[0][0]) {
    case 'i':
      key = atol(op[1]);
      value = (char*) nvm_alloc(VALUE_LEN);
      memcpy(value, op[2], strlen(op[2])+1);
			asm volatile ("clflush %0\n" : "+m" (*(char *)(value)));
      asm volatile("mfence" ::: "memory");

      witcher_tx_begin();
			ret = (char*) art_insert(tree, key, sizeof(uint64_t),(void*)value);
      witcher_tx_end();

      fprintf(output_file, "%s\n", ret);

      break;
    case 'g':
      key = atol(op[1]);

      witcher_tx_begin();
		  value = (char*) art_search(tree, key, sizeof(uint64_t));
      if (value != NULL) {
        strlen(value);
      }
      witcher_tx_end();

      fprintf(output_file, "%s\n", value);
      break;
  }
}

void read_op_and_run(char *op_file_path,
                     int start_index,
                     int skip_index,
                     char *output_file_path,
                     art_tree* tree) {
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

  art_tree *tree =init_wort(pmem_path, pmem_size_in_mib, layout_name);

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
