#include <iostream>
#include <chrono>
#include <random>
#include <vector>
#include <thread>
#include <atomic>
#include "tbb/tbb.h"

using namespace std;

#include "clht.h"
#include "ssmem.h"
#include "pmdk.h"



int main(int argc, char **argv) {

    char *path = "/mnt/pmem0/p_clht";
    size_t size_in_mb = 1024;
    if (argc != 3) {
	printf("usage : %s nData(integer) read/write(read_only=1,write=0)\n");		
    }

    uint64_t n = std::atoll(argv[1]);
    int read_after_re_execute= std::atoi(argv[2]);
    uint64_t *keys = new uint64_t[2*n];
    for(uint64_t i=0; i<2*n; i++)
	keys[i]=i;

    clht_t *hashtable = init_clht(path,size_in_mb,"p-clht");;
    //clht_t *hashtable = clht_create(512);
    if(!read_after_re_execute){
        printf("insert\n");	    
        for(uint64_t i=0; i<n; i++){
             clht_put(hashtable, keys[i], keys[i]);
        }
    }
   
    if(!read_after_re_execute){
         for(uint64_t i=0; i<n; i++){
        	 uintptr_t val = clht_get(hashtable->ht, keys[i]);
		 printf("val :%lu keys[%d]:%lu\n",val,i,keys[i]);
       	 	if (val != keys[i]) {
        	        std::cout << "[CLHT] wrong key read: " << val << "expected: " << keys[i] << std::endl;
               	  exit(1);
         	}
    	}
    }
    if(!read_after_re_execute){
    	for(uint64_t i=0; i<n/2; i++){
           clht_remove(hashtable, keys[i]);
    	}
    }
    if(read_after_re_execute){
        printf("insert\n");	    
        for(uint64_t i=n; i<2*n; i++){
             clht_put(hashtable, keys[i], keys[i]);
        }
        for(uint64_t i=n/2; i<2*n; i++){
            uintptr_t val = clht_get(hashtable->ht, keys[i]);
            printf("val :%lu keys[%d]:%lu\n",val,i,keys[i]);
            if (val != keys[i]) {
                 std::cout << "[CLHT] wrong key read: " << val << "expected: " << keys[i] << std::endl;
                 exit(1);
            }
        }
    }
    return 0;
}
