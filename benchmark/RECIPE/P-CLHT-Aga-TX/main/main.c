#include<stdio.h>
#include<string.h>
#include<assert.h>

#include "WitcherAnnotation.h"
#include "clht_lb_res.h"
#include "ssmem.h"

#define BUFF_LEN 2048

void run_op(char **op, clht_t* hashtable, FILE *output_file) {
  uint64_t key_num;
  uint64_t value_num;
  int ret;
  int size;
  uintptr_t val;
  clht_hashtable_t *ht;
  switch (op[0][0]) {
    case 'i':
      key_num = atol(op[1]);
      value_num = atol(op[2]);
      witcher_tx_begin();
      ret = clht_put(hashtable, key_num, value_num);
      witcher_tx_end();

      fprintf(output_file, "%d\n", ret);

      break;
    case 'd':
      key_num = atol(op[1]);
      witcher_tx_begin();
   	  val = clht_remove(hashtable, key_num);
      witcher_tx_end();

      fprintf(output_file, "%lu\n",val);
      break;
    case 'g':
      key_num = atol(op[1]);
      ht = (clht_hashtable_t*)clht_ptr_from_off(hashtable->ht_off);
      witcher_tx_begin();
   	  val = clht_get(ht,key_num);
      if (val != 0) {
        value_num = val;
      }
      witcher_tx_end();

      if (val != 0) {
        fprintf(output_file, "%lu\n", value_num);
      } else {
        fprintf(output_file, "(null)\n");
      }
      break;
    case 'z':
      ht = (clht_hashtable_t*)clht_ptr_from_off(hashtable->ht_off);
      witcher_tx_begin();
   	  size = clht_size(ht);
      witcher_tx_end();

      fprintf(output_file, "%d\n", size);
      break;
  }
}

void read_op_and_run(char *op_file_path,
                     int start_index,
                     int skip_index,
                     char *output_file_path,
                     clht_t *hashtable) {
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

    run_op(op, hashtable, output_file);

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
  clht_t *hashtable = clht_create(pmem_path, pmem_size_in_mib, 16);

  read_op_and_run(op_file_path,
                  start_index,
                  skip_index,
                  output_file_path,
                  hashtable);

  if (argc == 9) {
    char *memory_layout_path = argv[8];
    witcher_get_memory_layout(memory_layout_path);
  }

  return 0;
}
