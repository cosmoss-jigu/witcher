#include <iostream>
#include<stdio.h>
#include<string.h>
#include<assert.h>

#include "src/CCEH.h"
#include "WitcherAnnotation.h"

#define VALUE_LEN 8

#define CACHE_LINE_SIZE 64

inline void mfence()
{
  asm volatile("mfence":::"memory");
}

inline void clflush(char *data, int len)
{
  volatile char *ptr = (char *)((unsigned long)data &~(CACHE_LINE_SIZE-1));
  for(; ptr<data+len; ptr+=CACHE_LINE_SIZE){
    asm volatile("clflush %0" : "+m" (*(volatile char *)ptr));
  }
  mfence();
}

void run_op(char **op, CCEH *cceh, FILE *output_file) {
  size_t key;
  char* value;
  bool ret;
  switch (op[0][0]) {
    case 'i':
      key = atol(op[1]);
      value = (char*) nvm_alloc(VALUE_LEN);
      memcpy(value, op[2], strlen(op[2])+1);
      clflush(value, VALUE_LEN);

      witcher_tx_begin();
      cceh->Insert(key, value);
      witcher_tx_end();

      fprintf(output_file, "\n");
      break;
    case 'o':
      key = atol(op[1]);
      value = (char*) nvm_alloc(VALUE_LEN);
      memcpy(value, op[2], strlen(op[2])+1);
      clflush(value, VALUE_LEN);

      witcher_tx_begin();
      ret = cceh->InsertOnly(key, value);
      witcher_tx_end();

      fprintf(output_file, "%d\n", ret);
      break;
    case 'g':
      key = atol(op[1]);

      witcher_tx_begin();
      const char* rt_get = cceh->Get(key);
      witcher_tx_end();

      fprintf(output_file, "%s\n", rt_get);
      break;
  }
}

void read_op_and_run(char *op_file_path,
                     int start_index,
                     int skip_index,
                     char *output_file_path,
                     CCEH *cceh) {
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

    run_op(op, cceh, output_file);

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
  CCEH *cceh = init_CCEH(pmem_path, pmem_size_in_mib, layout_name);

  read_op_and_run(op_file_path,
                  start_index,
                  skip_index,
                  output_file_path,
                  cceh);

  if (argc == 9) {
    char *memory_layout_path = argv[8];
    witcher_get_memory_layout(memory_layout_path);
  }

  return 0;
}
