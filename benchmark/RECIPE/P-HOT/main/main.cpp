#include<stdio.h>
#include<string.h>
#include<assert.h>

#include <hot/rowex/HOTRowex.hpp>
#include <idx/contenthelpers/IdentityKeyExtractor.hpp>
#include <idx/contenthelpers/OptionalValue.hpp>
#include "WitcherAnnotation.h"

typedef struct IntKeyVal {
    uint64_t key;
    uintptr_t value;
} IntKeyVal;

template<typename ValueType = IntKeyVal *>
class IntKeyExtractor {
    public:
    typedef uint64_t KeyType;

    inline KeyType operator()(ValueType const &value) const {
        return value->key;
    }
};

using Tree = hot::rowex::HOTRowex<IntKeyVal*, IntKeyExtractor>;

void run_op(char **op, Tree* tree, FILE *output_file) {
  uint64_t key;
  uint64_t value;
  IntKeyVal *key_val;
  idx::contenthelpers::OptionalValue<IntKeyVal*> result;
  bool ret;
  switch (op[0][0]) {
    case 'i':
      key = atol(op[1]);
      value = atol(op[2]);
      nvm_aligned_alloc((void **)&key_val, 64, sizeof(IntKeyVal));
      key_val->key = key;
      key_val->value = value;
	    hot::commons::clflush((char *)key_val, sizeof(IntKeyVal));

      witcher_tx_begin();
      ret = tree->insert(key_val);
      witcher_tx_end();

      fprintf(output_file, "%d\n", ret);

      break;
    case 'u':
      key = atol(op[1]);
      value = atol(op[2]);
      nvm_aligned_alloc((void **)&key_val, 64, sizeof(IntKeyVal));
      key_val->key = key;
      key_val->value = value;
	    hot::commons::clflush((char *)key_val, sizeof(IntKeyVal));

      witcher_tx_begin();
      result = tree->upsert(key_val);
      witcher_tx_end();

      if (result.mIsValid) {
        fprintf(output_file, "%d %lu\n", result.mIsValid, result.mValue->value);
      } else {
        fprintf(output_file, "%d\n", result.mIsValid);
      }

      break;
    case 'g':
      key = atol(op[1]);

      witcher_tx_begin();
      result = tree->lookup(key);
      witcher_tx_end();

      if (result.mIsValid) {
        fprintf(output_file, "%d %lu\n", result.mIsValid, result.mValue->value);
      } else {
        fprintf(output_file, "%d\n", result.mIsValid);
      }

      break;
    case 'r':
      key = atol(op[1]);
      size_t range_num = atol(op[2]);

      witcher_tx_begin();
      result = tree->scan(key, range_num);
      witcher_tx_end();

      if (result.mIsValid) {
        fprintf(output_file, "%d %lu\n", result.mIsValid, result.mValue->value);
      } else {
        fprintf(output_file, "%d\n", result.mIsValid);
      }

      break;
  }
}

void read_op_and_run(char *op_file_path,
                     int start_index,
                     int skip_index,
                     char *output_file_path,
                     Tree* tree) {
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

Tree *init_P_HOT(char *path, size_t size, char *layout_name) {
  int is_created;
  root_obj *root_obj = init_nvmm_pool(path, size, layout_name, &is_created);
  if (is_created == 0) {
    printf("Reading from an existing p-hot.\n");
    Tree *tree = (Tree *)root_obj->p_hot_ptr;
    tree->recovery();
    return tree;
  }
  Tree *tree = new hot::rowex::HOTRowex<IntKeyVal *, IntKeyExtractor>;

  root_obj->p_hot_ptr= (void*)tree;
  hot::commons::clflush(reinterpret_cast <char *> (root_obj), sizeof(root_obj));
  return tree;
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
  Tree* tree = init_P_HOT(pmem_path, pmem_size_in_mib, layout_name);

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
