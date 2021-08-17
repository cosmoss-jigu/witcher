#include<stdio.h>
#include<string.h>
#include<assert.h>

#include "ctree_map.h"
#include "WitcherAnnotation.h"

#define VALUE_LEN 8

#define BUFF_LEN 2048

// 1 MiB
#define MIB_SIZE ((size_t)(1024 * 1024))

typedef struct root {
	TOID(struct ctree_map) map;
} root_obj;

static PMEMobjpool *pop;
static TOID(struct ctree_map) map;

void run_op(char **op, FILE *output_file) {
  uint64_t key;
  PMEMoid value;
  int ret;
  switch (op[0][0]) {
    case 'i':
      key = atol(op[1]);
      ret = pmemobj_alloc(pop, &value, VALUE_LEN, 0, NULL, NULL);
      if (ret) {
        perror("nvmm memory allocation failed\n");
        exit(1);
      }
      memcpy(pmemobj_direct(value), op[2], strlen(op[2])+1);
      void *value_addr = pmemobj_direct(value);
      asm volatile("clflush %0" : "+m" (*(volatile char *)value_addr));
      asm volatile("mfence":::"memory");

      witcher_tx_begin();
      ret = ctree_map_insert(pop, map, key, value);
      witcher_tx_end();

      fprintf(output_file, "%d\n", ret);

      break;
    case 'd':
      key = atol(op[1]);

      witcher_tx_begin();
      value = ctree_map_remove(pop, map, key);
      witcher_tx_end();

      fprintf(output_file, "%s\n", pmemobj_direct(value));
      break;
    case 'g':
      key = atol(op[1]);

      witcher_tx_begin();
      value = ctree_map_get(pop, map, key);
      witcher_tx_end();

      fprintf(output_file, "%s\n", pmemobj_direct(value));
      break;
  }
}

void read_op_and_run(char *op_file_path,
                     int start_index,
                     int skip_index,
                     char *output_file_path) {
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

    run_op(op, output_file);

    count++;
  }
  fclose(op_file);
  fclose(output_file);
}

void init_map(char* pmem_path, size_t pmem_size_in_mib, char* layout_name) {
	if (access(pmem_path, F_OK) != 0) {
		pop = pmemobj_create(pmem_path, layout_name,
                         pmem_size_in_mib*MIB_SIZE, 0666);
		if (pop == NULL) {
      perror("failed to create pool\n");
			exit(1);
		}

		PMEMoid root = pmemobj_root(pop, sizeof(root_obj));
    root_obj* root_ptr = (root_obj*) pmemobj_direct(root);
	  ctree_map_new(pop, &root_ptr->map, NULL);
		map = root_ptr->map;
	} else {
		pop = pmemobj_open(pmem_path, layout_name);
		if (pop == NULL) {
      perror("failed to create pool\n");
			exit(1);
		}

		PMEMoid root = pmemobj_root(pop, sizeof(root_obj));
    root_obj* root_ptr = (root_obj*) pmemobj_direct(root);
		map = root_ptr->map;
	}
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
  init_map(pmem_path, pmem_size_in_mib, layout_name);

  read_op_and_run(op_file_path,
                  start_index,
                  skip_index,
                  output_file_path);

  if (argc == 9) {
    char *memory_layout_path = argv[8];
    witcher_get_memory_layout(memory_layout_path);
  }

  return 0;
}
