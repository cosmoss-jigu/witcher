#include <iostream>
#include <chrono>
#include <random>
#include "tbb/tbb.h"
#include "src/pmdk.h"

using namespace std;

#include "src/bwtree.h"

using namespace wangziqi2013::bwtree;

/*
 * class KeyComparator - Test whether BwTree supports context
 *                       sensitive key comparator
 *
 * If a context-sensitive KeyComparator object is being used
 * then it should follow rules like:
 *   1. There could be no default constructor
 *   2. There MUST be a copy constructor
 *   3. operator() must be const
 *
 */
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

/*
 * class KeyEqualityChecker - Tests context sensitive key equality
 *                            checker inside BwTree
 *
 * NOTE: This class is only used in KeyEqual() function, and is not
 * used as STL template argument, it is not necessary to provide
 * the object everytime a container is initialized
 */
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

TreeType *init_P_BWTREE(char *path,
                    size_t size,
                     char *layout_name)
{
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

int main(int argc, char **argv) {
    char *path = "/mnt/pmem0/p_bwtree";
    size_t size_in_mb = 1024;
    if (argc != 3) {
	printf("usage : %s nData(integer) read/write(read_only=1,write=0)\n");		
    }

    uint64_t n = std::atoll(argv[1]);
    int read_only = std::atoi(argv[2]);
    uint64_t *keys = new uint64_t[n];
    for(uint64_t i=0; i<n; i++)
	keys[i]=i;


 //    else{
    //auto t = new BwTree<uint64_t, uint64_t, KeyComparator, KeyEqualityChecker> {true, KeyComparator{1}, KeyEqualityChecker{1}};
    auto t = init_P_BWTREE(path,size_in_mb,"p_bwtree");

    t->UpdateThreadLocal(1);
    t->AssignGCID(0);
    std::atomic<int> next_thread_id;

    if(!read_only){
printf("insert \n");
        for(uint64_t i=0; i<n; i++){
            t->Insert(keys[i], keys[i]);
        }
        for(uint64_t i=0; i<n/2; i++){
            t->Delete(keys[i], keys[i]);
        }
    }
printf("read\n");
    std::vector<uint64_t> v{};
    v.reserve(1);
    if(read_only){
       for(uint64_t i=0; i<n/2; i++){
            t->Insert(keys[i], keys[i]);
       }
       for(uint64_t i=0; i<n; i++){
	    v.clear();
	    t->GetValue(keys[i], v);
            printf("v[0] %lu keys[%d] %lu \n",v[0],i,keys[i]);
	    if (v[0] != keys[i]) {
           	std::cout << "[BwTree] wrong value read: " << v[0] << " expected:" << keys[i] << std::endl;
            }
       }
    }
//scan

    auto it = t->Begin(keys[0]) ;
    int num_result= n;
    int resultsFound = 0;
    uint64_t buf[200];
    while (it.IsEnd() != true && resultsFound != num_result) {
        buf[resultsFound] = it->second;
        resultsFound++;
        it++;
    }

    //scan result;
    for(int i=0; i<resultsFound; i++)
	    printf("restuls %d  : %lu\n",i,buf[i]);

    return 0;
}
