#include <iostream>
#include <chrono>
#include <random>
#include "tbb/tbb.h"

using namespace std;

#include "masstree.h"

/*void run(char **argv) {
    std::cout << "Simple Example of P-Masstree" << std::endl;

    uint64_t n = std::atoll(argv[1]);
    uint64_t *keys = new uint64_t[n];

    // Generate keys
    for (uint64_t i = 0; i < n; i++) {
        keys[i] = i + 1;
    }

    int num_thread = atoi(argv[2]);
    tbb::task_scheduler_init init(num_thread);

    printf("operation,n,ops/s\n");
    masstree::leafnode *init_root = new masstree::leafnode(0);
    masstree::masstree *tree = new masstree::masstree(init_root);

    {
        // Build tree
        auto starttime = std::chrono::system_clock::now();
        tbb::parallel_for(tbb::blocked_range<uint64_t>(0, n), [&](const tbb::blocked_range<uint64_t> &range) {
            for (uint64_t i = range.begin(); i != range.end(); i++) {
                tree->put(keys[i], &keys[i]);
            }
        });
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now() - starttime);
        printf("Throughput: insert,%ld,%f ops/us\n", n, (n * 1.0) / duration.count());
        printf("Elapsed time: insert,%ld,%f sec\n", n, duration.count() / 1000000.0);
    }

    {
        // Lookup
        auto starttime = std::chrono::system_clock::now();
        tbb::parallel_for(tbb::blocked_range<uint64_t>(0, n), [&](const tbb::blocked_range<uint64_t> &range) {
            for (uint64_t i = range.begin(); i != range.end(); i++) {
                uint64_t *ret = reinterpret_cast<uint64_t *> (tree->get(keys[i]));
                if (*ret != keys[i]) {
                    std::cout << "wrong value read: " << *ret << " expected:" << keys[i] << std::endl;
                    throw;
                }
            }
        });
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now() - starttime);
        printf("Throughput: lookup,%ld,%f ops/us\n", n, (n * 1.0) / duration.count());
        printf("Elapsed time: lookup,%ld,%f sec\n", n, duration.count() / 1000000.0);
    }

    delete[] keys;
}*/

int main(int argc, char **argv) {

    char *path = "/mnt/pmem0/p_mt";
    size_t size_in_mb = 1024;
    if (argc != 3) {
	printf("usage : %s nData(integer) read/write(read_only=1,write=0)\n");		
	return 1;
    }
    masstree::masstree *tree = masstree::init_P_MASSTREE(path,size_in_mb,"p_mtt");
    printf("%p\n",tree);

    uint64_t n = std::atoll(argv[1]);
    uint64_t *keys = new uint64_t[2*n];
    int read_after_re_execute= std::atoi(argv[2]);

    // Generate keys
    for (uint64_t i = 0; i < 2*n; i++) {
        keys[i] = 2*i+ 1;
    }

    if(!read_after_re_execute){
	printf("insert \n");
	for (uint64_t i = 0; i < n; i++) {
       	    tree->put(keys[i], (void*)keys[i]);
    	}
    }

    for (uint64_t i = 0; i < n; i++) {
        uint64_t ret = reinterpret_cast<uint64_t> (tree->get(keys[i]));
	printf("*ret[%lu] :%lu keys[%lu]:%lu\n",i,(uint64_t)ret,i,keys[i]);
        if (ret != keys[i]) {
            std::cout << "wrong value read: " << ret << " expected:" << keys[i] << std::endl;
            throw;
        }
    }
    
    // when n is 100, this example will not work without recovery function.
    if(read_after_re_execute){
	printf("insert \n");
	for (uint64_t i = n; i < 2*n; i++) {
       	    tree->put(keys[i], (void*)keys[i]);
    	}
    }
    if(read_after_re_execute){
        for (uint64_t i = 0; i < 2*n; i++) {
            uint64_t ret = reinterpret_cast<uint64_t> (tree->get(keys[i]));
            printf("*ret[%lu] :%lu keys[%lu]:%lu\n",i,(uint64_t)ret,i,keys[i]);
            if (ret != keys[i]) {
                std::cout << "wrong value read: " << ret << " expected:" << keys[i] << std::endl;
                throw;
            }
        }
    }


//    run(argv);
    return 0;
}
