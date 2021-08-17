#include<stdio.h>
#include<string.h>
#include<assert.h>

#include "tbb/tbb.h"
#include "pmdk.h"
#include "WitcherAnnotation.h"

#include "bwtree.h"

using namespace wangziqi2013::bwtree;

class KeyComparator {
 public:
  inline bool operator()(const long int k1, const long int k2) const {
    return k1 < k2;
  }

  inline bool operator()(const uint64_t k1, const uint64_t k2) const {
      return k1 < k2;
  }

  inline bool operator()(const char *k1, const char *k2) const {
      return memcmp(k1, k2, strlen(k1) > strlen(k2) ? strlen(k1) : strlen(k2)) < 0;
  }

  KeyComparator(int dummy) {
    (void)dummy;

    return;
  }

  KeyComparator() = delete;
  //KeyComparator(const KeyComparator &p_key_cmp_obj) = delete;
};

class KeyEqualityChecker {
 public:
  inline bool operator()(const long int k1, const long int k2) const {
    return k1 == k2;
  }

  inline bool operator()(uint64_t k1, uint64_t k2) const {
      return k1 == k2;
  }

  inline bool operator()(const char *k1, const char *k2) const {
      if (strlen(k1) != strlen(k2))
          return false;
      else
          return memcmp(k1, k2, strlen(k1)) == 0;
  }

  KeyEqualityChecker(int dummy) {
    (void)dummy;

    return;
  }

  KeyEqualityChecker() = delete;
  //KeyEqualityChecker(const KeyEqualityChecker &p_key_eq_obj) = delete;
};

using TreeType = BwTree<uint64_t,
                        uint64_t,
                        KeyComparator,
                        KeyEqualityChecker>;

TreeType *init_P_BWTREE(char *path, size_t size, char *layout_name) {
  int is_created;
  root_obj *root_obj = init_nvmm_pool(path, size, layout_name, &is_created);
  if (is_created == 0) {
    printf("Reading from an existing p-bwtree.\n");
    TreeType *tree = (TreeType*)root_obj->p_bwtree_ptr;
    tree->re_init();
    return tree;
  }
  TreeType *tree = new BwTree<uint64_t, uint64_t, KeyComparator, KeyEqualityChecker> {true, KeyComparator{1}, KeyEqualityChecker{1}};

  root_obj->p_bwtree_ptr= (void*)tree;
  clflush((char *)root_obj, sizeof(root_obj),true,true);
  return tree;
}

int range(TreeType* tree, uint64_t key_num, int range_num, uint64_t* buf) {
  int resultsFound = 0;
  auto it = tree->Begin(key_num);
  while (it.IsEnd() != true && resultsFound != range_num) {
    buf[resultsFound] = it->second;
    resultsFound++;
    it++;
  }
  return resultsFound;
}

void run_op(char **op, TreeType* tree, std::vector<uint64_t> v, FILE *output_file) {
  uint64_t key_num;
  uint64_t value_num;
  int ret;
  int range_num;
  switch (op[0][0]) {
    case 'i':
      key_num = atol(op[1]);
      value_num = atol(op[2]);

      witcher_tx_begin();
      tree->Insert(key_num, value_num);
      witcher_tx_end();

      fprintf(output_file, "\n");
      break;

    case 'd':
      key_num = atol(op[1]);

      witcher_tx_begin();
   	  tree->Delete(key_num, key_num);
      witcher_tx_end();

      fprintf(output_file, "\n");
      break;

    case 'g':
      key_num = atol(op[1]);

      v.clear();

      witcher_tx_begin();
   	  tree->GetValue(key_num,v);
      if (v.size() != 0) {
        value_num = v[0];
      }
      witcher_tx_end();

      if (v.size() != 0) {
        fprintf(output_file, "%lu\n", value_num);
      } else {
        fprintf(output_file, "(null)\n");
      }
      break;

    case 'r':
      key_num = atol(op[1]);
      range_num = atoi(op[2]);
      uint64_t buf[range_num];

      witcher_tx_begin();
      int resultsFound = range(tree, key_num, range_num, buf);
      witcher_tx_end();

      for(int i=0; i < resultsFound; i++) {
        fprintf(output_file, "%lu ", buf[i]);
      }
      fprintf(output_file, "\n");
      break;
  }
}

void read_op_and_run(char *op_file_path,
                     int start_index,
                     int skip_index,
                     char *output_file_path,
                     TreeType* tree) {
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

    std::vector<uint64_t> v{};
    v.reserve(1);
    run_op(op, tree, v, output_file);

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
  auto *tree = init_P_BWTREE(pmem_path,
                          pmem_size_in_mib,
                          layout_name);

  tree->UpdateThreadLocal(1);
  tree->AssignGCID(0);

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
