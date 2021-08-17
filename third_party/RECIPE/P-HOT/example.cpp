#include <iostream>
#include <chrono>
#include <random>
#include "tbb/tbb.h"

using namespace std;

#include <hot/rowex/HOTRowex.hpp>
#include <idx/contenthelpers/IdentityKeyExtractor.hpp>
#include <idx/contenthelpers/OptionalValue.hpp>

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

using TreeType = hot::rowex::HOTRowex<IntKeyVal*,
                        IntKeyExtractor>;


TreeType *init_P_HOT(char *path,
                    size_t size,
                     char *layout_name)
{
   	 int is_created;
	 root_obj *root_obj = init_nvmm_pool(path, size, layout_name, &is_created);
   	 if (is_created == 0) {
	      printf("Reading from an existing p-hot.\n");
	      TreeType *tree = (TreeType *)root_obj->p_hot_ptr;
	      tree->recovery();
 	      return tree;
    	}
    	TreeType *tree = new hot::rowex::HOTRowex<IntKeyVal *, IntKeyExtractor>;
	printf("size of tree : %d\n",sizeof(*tree));

    	root_obj->p_hot_ptr= (void*)tree;
	hot::commons::clflush(reinterpret_cast <char *> (root_obj), sizeof(root_obj));
    	return tree;
}

int main(int argc, char **argv) {
    char *path = "/mnt/pmem0/p_hot";
    size_t size_in_mb = 1024;
    if (argc != 3) {
	printf("usage : %s nData(integer) read/write(read_only=1,write=0)\n");		
    }

    auto mTrie = init_P_HOT(path,size_in_mb,"p_hot");
    printf("mTrie:%p\n",mTrie);

    uint64_t n = std::atoll(argv[1]);
    int read_only = std::atoi(argv[2]);
    uint64_t *keys = new uint64_t[2*n];

 //   auto t = tt.getThreadInfo();

    // Generate keys
    for (uint64_t i = 0; i < 2*n; i++) {
        keys[i] = i + 1;
    }

    //Insert
    if(read_only == 0){
	printf("insert \n");
        for (uint64_t i = 0; i < n; i++) {
            IntKeyVal *key;
            nvm_aligned_alloc((void **)&key, 64, sizeof(IntKeyVal));
            key->key = keys[i];
            key->value = keys[i];
            if (!(mTrie->insert(key))) {
                fprintf(stderr, "[HOT] insert faile\n");
                exit(1);
            }
	    printf("key[%d] :%lu inserted\n",i,keys[i]);
    	}
    }
    if(read_only == 0){
         for (uint64_t i = 0; i < n; i++) {
              idx::contenthelpers::OptionalValue<IntKeyVal *> result = mTrie->lookup(keys[i]);
              if (!result.mIsValid || result.mValue->value != keys[i]) {
                   printf("mIsValid = %d\n", result.mIsValid);
                   printf("Return value = %lu, Correct value = %lu\n", result.mValue->value, keys[i]);
                   exit(1);
              }
        	 printf("key[%d] :%lu inserted\n",i,result.mValue->value);
    	
         }
    }
 
    uint64_t update_value =1000000;
    if(read_only == 1){
         for (uint64_t i = n; i < 2*n; i++) {
             IntKeyVal *key;
             nvm_aligned_alloc((void **)&key, 64, sizeof(IntKeyVal));
             key->key = keys[i];
             key->value = keys[i];
	     if (!(mTrie->insert(key))) {
                fprintf(stderr, "[HOT] insert faile\n");
                exit(1);
             }
	 }
    }
//    uint64_t update_value =0;
    if(read_only == 1){
         for (uint64_t i = 0; i < n; i++) {
             IntKeyVal *key;
             nvm_aligned_alloc((void **)&key, 64, sizeof(IntKeyVal));
             key->key = keys[i];
             key->value = keys[i]+update_value;
             idx::contenthelpers::OptionalValue<IntKeyVal *> result = mTrie->upsert(key);
        	if (!result.mIsValid){
         		printf("Return value = %lu, Correct value = %lu\n", result.mValue->value, keys[i]);
	       }
	 }
    }

    //Read Keys from pool
    printf("read\n");
    if(read_only==1){
	    for (uint64_t i = 0; i < 2*n; i++) {
       		  idx::contenthelpers::OptionalValue<IntKeyVal *> result = mTrie->lookup(keys[i]);

		  uint64_t expected_result = 0;
		  if(i<n) expected_result = keys[i]+update_value;
		  else expected_result = keys[i]; 
       		  if (!result.mIsValid || result.mValue->value != expected_result) {
               		 printf("mIsValid = %d\n", result.mIsValid);
	                printf("Return value = %lu, Correct value = %lu\n", result.mValue->value, expected_result);
       		         exit(1);
	         }
		 printf("key[%d] :%lu inserted\n",i,result.mValue->value);
	    }	
    }
    //scan 

    idx::contenthelpers::OptionalValue<IntKeyVal *> result = mTrie->scan(keys[0], n/2);
    if(read_only==0)
	    printf("Start Key :  %lu, range : %d Last Key in the results :%lu expected value: %lu\n",keys[0], n/2, result.mValue->value, keys[n/2]); 
    else
	    printf("Start Key :  %lu, range : %d Last Key in the results :%lu expected value: %lu\n",keys[0], n/2, result.mValue->value, keys[n/2]+update_value); 

    return 0;
}
