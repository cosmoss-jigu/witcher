#include<stdio.h>
#include<string.h>
#include<assert.h>

#include "Tree.h"
#include "WitcherAnnotation.h"

#define BUFF_LEN 2048

using namespace ART_ROWEX;

void run_op(char **op, Tree* tree, ThreadInfo t, FILE *output_file) {
  uint64_t key_num;
  uint64_t key_num_min;
  uint64_t key_num_max;
  Key* key;
  Key* key_min;
  Key* key_max;
  uint64_t value_num;
  uint64_t* value;
  int ret;
  switch (op[0][0]) {
    case 'i':
      key_num = atol(op[1]);
      value_num = atol(op[2]);

      witcher_tx_begin();
   	  key = key->make_leaf(key_num, sizeof(uint64_t), value_num);
	    tree->insert(key, t);
      witcher_tx_end();

      fprintf(output_file, "\n");

      break;
    case 'd':
      key_num = atol(op[1]);

      witcher_tx_begin();
   	  key = key->make_leaf(key_num, sizeof(uint64_t), 0);
	    tree->remove(key, t);
      witcher_tx_end();

      fprintf(output_file, "\n");
      break;
    case 'g':
      key_num = atol(op[1]);

      witcher_tx_begin();
   	  key = key->make_leaf(key_num, sizeof(uint64_t), 0);
	    value = (uint64_t*) tree->lookup(key, t);
      if (value != NULL) {
        value_num = *value;
      }
      witcher_tx_end();

      if (value != NULL) {
        fprintf(output_file, "%lu\n", value_num);
      } else {
        fprintf(output_file, "(null)\n");
      }
      break;
    case 'r':
      key_num_min = atol(op[1]);
      key_num_max = atol(op[2]);

      Key *results[BUFF_LEN];
      for (int i = 0; i < BUFF_LEN; i++) {
        results[i] = NULL;
      }
      Key *continueKey = NULL;
      size_t resultsFound = 0;
      size_t resultsSize = BUFF_LEN;

      witcher_tx_begin();
   	  key_min = key->make_leaf(key_num_min, sizeof(uint64_t), 0);
   	  key_max = key->make_leaf(key_num_max, sizeof(uint64_t), 0);
	    tree->lookupRange(key_min, key_max, continueKey, results, resultsSize, resultsFound, t);
      witcher_tx_end();

      for (int i = 0; i < BUFF_LEN; i++) {
        if (results[i] == NULL) {
          break;
        }
        fprintf(output_file, "%lu ", results[i]->value);
      }
      fprintf(output_file, "\n");

      break;
  }
}

void read_op_and_run(char *op_file_path,
                     int start_index,
                     int skip_index,
                     char *output_file_path,
                     Tree* tree,
                     ThreadInfo t) {
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

    run_op(op, tree, t, output_file);

    count++;
  }
  fclose(op_file);
  fclose(output_file);
}

void loadKey(TID tid, Key &key) {
    return ;
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
  Tree *tree = init_P_ART(pmem_path,
                          pmem_size_in_mib,
                          layout_name);
  Tree tt(loadKey); // Dummy tree to get the threadinfo.. Need to talk with Xinwei
  ThreadInfo t = tt.getThreadInfo();

  read_op_and_run(op_file_path,
                  start_index,
                  skip_index,
                  output_file_path,
                  tree,
                  t);

  if (argc == 9) {
    char *memory_layout_path = argv[8];
    witcher_get_memory_layout(memory_layout_path);
  }

  exit(0);
}
