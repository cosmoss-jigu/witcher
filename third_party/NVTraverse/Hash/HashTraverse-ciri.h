#ifndef HASH_TRAVERSE_H_
#define HASH_TRAVERSE_H_

#include "Utilities.h"
#include "pmdk.h"
#include "ListTraverse-ciri.h"
#include <vector>
#include <assert.h>

template <class T> class HashTraverse{
public:
    
  void* operator new(size_t size) {
    void* ret = nvm_alloc(size);
    return ret;
  }
    
    HashTraverse(int s) {
        size = s;
	//buckets = static_cast<ListGenNewGC<T>** >(ssmem_alloc(allocW, s * sizeof(ListGenNewGC<T>*)));
        buckets = new ListTraverse<T>*[s];
        buckets = (ListTraverse<T>**) nvm_alloc(sizeof(ListTraverse<T>*) * s);
	for (int i = 0; i< size; i++) {
                buckets[i] = new ListTraverse<int>();

		//buckets[i] =  static_cast<ListIz<T>* >(ssmem_alloc(allocB, sizeof(ListIz<T>)));
        }
        
    }
    
    bool insert(int k, T item) {
        int index = k%size;
        bool b = buckets[index]->insert(k, item);
        return b;
    }
    
    bool remove(int k) {
        int index = k%size;
        bool b = buckets[index]->remove(k);
        return b;
    }
    
    T contains(int k) {
        int index = k%size;
        T b = buckets[index]->contains(k);
        return b;
    }
    
    //========================================
    
    
private:
    int size;
    int padding[PADDING];
    ListTraverse<T>** buckets;
    
};

HashTraverse<int > *init_hash(char *path,
                              size_t size,
                              char *layout_name)
{
    int is_created;
    root_obj *root_obj = init_nvmm_pool(path, size, layout_name, &is_created);
    if (is_created == 0) {
      printf("Reading from an existing Hash.\n");
      HashTraverse<int>* ret = (HashTraverse<int>*) root_obj->ptr;
      return ret;
    }
    HashTraverse<int>* hash = new HashTraverse<int>(512);

    root_obj->ptr = hash;
    FLUSH((char *)root_obj);
    SFENCE();
    return hash;
}

#endif /* HASH_IZ_H_ */

