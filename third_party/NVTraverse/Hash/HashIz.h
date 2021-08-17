#ifndef HASH_IZ_H_
#define HASH_IZ_H_

#include "../Utilities.h"
#include "../List/ListIz.h"
#include <vector>
#include <assert.h>

template <class T> class HashIz{
public:    
    
    HashIz(int s) {
        size = s;
	//buckets = static_cast<ListIz<T>** >(ssmem_alloc(allocW, s * sizeof(ListIz<T>*)));
        buckets = new ListIz<T>*[s];
	for (int i = 0; i< size; i++) {
                buckets[i] = new ListIz<int>();

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
    
    bool contains(int k) {
        int index = k%size;
        bool b = buckets[index]->contains(k);
        return b;
    }
    
    //========================================
    
    
private:
    int size;
    int padding[PADDING];
    ListIz<T>** buckets;
    
};

#endif /* HASH_IZ_H_ */

