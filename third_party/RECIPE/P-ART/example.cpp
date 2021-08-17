#include <iostream>
#include <chrono>
#include <random>
#include "tbb/tbb.h"

using namespace std;

#include "Tree.h"

void loadKey(TID tid, Key &key) {
    return ;
}

int main(int argc, char **argv) {
    char *path = "/mnt/pmem0/p_art";
    size_t size_in_mb = 1024;
    if (argc != 3) {
	printf("usage : %s nData(integer) read/write(read_only=1,write=0)\n");		
    }
    ART_ROWEX::Tree *tree = ART_ROWEX::init_P_ART(path,size_in_mb,"p_art");
//    auto t = tree->getThreadInfo();


    ART_ROWEX::Tree tt(loadKey); // Dummy tree to get the threadinfo.. Need to talk with Xinwei
    auto t = tt.getThreadInfo();
    printf("%p\n",tree);

    uint64_t n = std::atoll(argv[1]);
    int read_only = std::atoi(argv[2]);
    uint64_t *keys = new uint64_t[n];
    std::vector<Key *> Keys;

    Keys.reserve(n);
    printf("here?\n");
 //   auto t = tt.getThreadInfo();

    // Generate keys
    for (uint64_t i = 0; i < n; i++) {
        keys[i] = i + 1;
    }

    //Insert
    if(read_only == 0){
	printf("insert \n");
        for (uint64_t i = 0; i < n; i++) {
   	     Keys[i] = Keys[i]->make_leaf(keys[i], sizeof(uint64_t), keys[i]);
	     tree->insert(Keys[i], t);
    	}
    }
    
    
    if(read_only == 1){
	for (uint64_t i = 0; i < n; i++) {
		Keys[i] = Keys[i]->make_leaf(keys[i], sizeof(uint64_t), keys[i]);
    	}
    }
    //Read Keys from pool
    for (uint64_t i = 0; i < n; i++) {
        uint64_t *val = reinterpret_cast<uint64_t *> (tree->lookup(Keys[i], t));

	printf("*ret[%lu] :%lu keys[%lu]:%lu\n",i,*(uint64_t*)val,i,keys[i]);
        if (*val != keys[i]) {
              std::cout << "wrong value read: " << *val << " expected:" << keys[i] << std::endl;
              throw;
        }
    }
    

/*    if (argc != 3) {
        printf("usage: %s [n] [nthreads]\nn: number of keys (integer)\nnthreads: number of threads (integer)\n", argv[0]);
        return 1;
    }

   // run(argv);*/
    return 0;
}
