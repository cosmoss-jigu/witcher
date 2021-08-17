#include <stdio.h>
#include <stdint.h>
#include "wort.h"

int main(int argc, char **argv){
	int i, nData,readOnly;
	nData = atoi(argv[1]);
	readOnly = atoi(argv[2]);

        char *path = "/mnt/pmem0/wort";
        size_t size_in_mb = 1024;
        if (argc != 3) {
  		printf("usage : %s nData(integer) read/write(read_only=1,write=0)\n");		
		return 1;
	}
        art_tree *t=init_wort(path,size_in_mb,"wort");
	uint64_t *keys;
	keys = (uint64_t *)malloc(sizeof(uint64_t)*nData);
	for (i=0; i<nData; i++){
		keys[i]=i;
	}

	if(!readOnly){
		for(i=0; i<nData; i++)
			art_insert(t, keys[i], sizeof(uint64_t),(void*)keys[i]);
	}

	for (i=0; i<nData; i++){
		void * ret;
		ret = art_search(t, keys[i], sizeof(uint64_t));
		printf("ret :%lu, key[%d]:%lu \n",(uint64_t)ret, i,keys[i]);
	}

	return 0;
}


