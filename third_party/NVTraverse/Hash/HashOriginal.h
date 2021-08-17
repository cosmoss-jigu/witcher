#ifndef HashOriginal_h
#define HashOriginal_h

#include "../Utilities.h"
#include "../List/ListOriginal.h"
#include <vector>
#include <assert.h>

template <class T> class HashOriginal{
public:
    
    HashOriginal(int s) {
        size = s;
	//buckets = static_cast<ListOriginal<T>** >(ssmem_alloc(alloc, s * sizeof(ListOriginal<T>* )));
        buckets = new ListOriginal<T>*[s];
	for (int i = 0; i< size; i++) {
	    buckets[i] = new ListOriginal<int>();
		//buckets[i] =  static_cast<ListOriginal<T>* >(ssmem_alloc(allocB, sizeof(ListOriginal<T>)));
        }
    }
    
    bool insert(int k, T item) {
        //std:: cout << "k: " << k << std::endl;
        int index = k%size;
        //iistd:: cout << "index: " << index << std::endl;
        
        bool b = buckets[index]->insert(k, item);
        //std:: cout << "res: " << b << std::endl;
        
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
    ListOriginal<T>** buckets;
    
};

#endif /* HASH_ORIGINAL_H_ */

