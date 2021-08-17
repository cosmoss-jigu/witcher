#ifndef _SKIPLIST_TRAVERSE_H_
#define _SKIPLIST_TRAVERSE_H_

#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include "rand_r_32.h"
#include "../Utilities.h"

#define FRASER_MAX_MAX_LEVEL 64 /* covers up to 2^64 elements */


extern __thread unsigned long * seeds;
extern __thread void* nodes[1024];
extern __thread ssmem_allocator_t* alloc;
extern int levelmax;


/*int floor_log_2(unsigned int n)
{
        int pos = 0;
        if (n >= 1 << 16)
        {
                n >>= 16;
                pos += 16;
        }
        if (n >= 1 << 8)
        {
                n >>= 8;
                pos += 8;
        }
        if (n >= 1 << 4)
        {
                n >>= 4;
                pos += 4;
        }
        if (n >= 1 << 2)
        {
                n >>= 2;
                pos += 2;
        }
        if (n >= 1 << 1)
        {
                pos += 1;
        }
        return ((n == 0) ? (-1) : pos);
}


*/

template <class T> class SkiplistTraverse {
    public:
        class Node {
	    public:
	        int key;
		T val;
		unsigned char toplevel;
		std::atomic<Node*> next[FRASER_MAX_MAX_LEVEL];
		
        	Node() {}

		Node(int k, T v, int topl) {
        	    key = key;
        	    val = v;
        	    toplevel = topl;
		    //next = new std::atomic<Node*>[levelmax];
        	    //FLUSH(this);
		}
		Node(int k, T v, Node* n, int topl) {
		    key = k;
                    val = v;
                    toplevel = topl;
		    //next = new std::atomic<Node*>[levelmax];
        	    for (int i = 0; i < levelmax; i++)
        	    {
                	next[i].store(n);
                	//FLUSH(&next[i]); depends whether it fits in a single cacheline
        	    }
        	    //FLUSH(this);
		}
		void set(int k, T v, Node* n, int topl) {
		    key = k;
                    val = v;
                    toplevel = topl;
                    for (int i = 0; i < levelmax; i++)
                    {
                        next[i].store(n);
                    }
		}
        	bool CASNext(Node* exp, Node* n, int i) {
        	    Node* old = next[i].load();
                    if (exp != old) { 
                	BARRIER(&next[i]);
                	return false;
            	    }
            	    bool ret = next[i].compare_exchange_strong(exp, n);
            	    BARRIER(&next[i]);
            	    return ret;
        	}
        	Node* getNextF(int i) {
            	    Node* n = next[i].load();
		    if (n)
            	        BARRIER(&next[i]);
            	    return n;  
	        }
		Node* getNext(int i) {
                    Node* n = next[i].load();
                    return n;
                }
	};

	SkiplistTraverse() {
            Node *min, *max;
            //max = static_cast<Node*>(ssmem_alloc(alloc, sizeof(Node)));
	    max = new Node();
	    max->set(INT_MAX, 0, nullptr, levelmax);
	    BARRIER(max);
	    //min = static_cast<Node*>(ssmem_alloc(alloc, sizeof(Node)));
            min = new Node();
	    min->set(INT_MIN, 0, max, levelmax);
	    BARRIER(min);
            this->head = min;
	    BARRIER(&this->head);
	}
	SkiplistTraverse(T val) {
	    Node *min, *max;
            //max = static_cast<Node*>(ssmem_alloc(alloc, sizeof(Node)));
            max = new Node();
	    max->set(INT_MAX, 0, nullptr, levelmax);
            BARRIER(max);
            //min = static_cast<Node*>(ssmem_alloc(alloc, sizeof(Node)));
            min = new Node();
	    min->set(INT_MIN, val, max, levelmax);
            BARRIER(min);
            this->head = min;
            BARRIER(&this->head);
	}
	~SkiplistTraverse() {       
	    Node *node, *next;
            node = this->head;
            while (node != nullptr)
            {
                next = node->getNext(0);
                //delete node;
                node = next;
            }
            // ssfree(set);
	}
	int size() {
            int size = 0;
            Node *node;
            node = static_cast<Node *>(getCleanReference(this->head->getNext(0)));
            while (node->getNext(0) != nullptr) {
                if (!isMarked(node->getNext(0))) {
                    size++;
                }
                node = static_cast<Node *>(getCleanReference(node->getNext(0)));
            }
            return size;
        }
	T get(int key) {
	    Node *succs[FRASER_MAX_MAX_LEVEL], *preds[FRASER_MAX_MAX_LEVEL];
	    bool exists = search_no_cleanup(key, preds, succs);
	    FLUSH(preds[0]);
	    FLUSH(succs[0]);
            SFENCE();
	    if (exists) {
                return succs[0]->val;
	    } else {
		return 0;
	    }
	}
	bool contains(int key) {
	    return get(key) != 0;
	}
	bool remove(int key) {
	    Node *succs[FRASER_MAX_MAX_LEVEL];
            bool found = search(key, nullptr, succs);
            if (!found) {
                SFENCE();
                return false;
            }
            Node *node_del = succs[0];
            bool my_delete = mark_node_ptrs(node_del);
            if (my_delete) {
                search(key, nullptr, nullptr);
                SFENCE();
                return true;
            }
            SFENCE();
            return false;
	}

	bool insert(int key, T val, int id) {
            Node *newNode, *pred, *succ;
            Node *succs[FRASER_MAX_MAX_LEVEL], *preds[FRASER_MAX_MAX_LEVEL];
            int i;
            bool found;

	    retry:
                found = search(key, preds, succs);
        	if (found) {
            	    SFENCE();
                    return false;
        	}
                newNode = static_cast<Node*>(ssmem_alloc(alloc, sizeof(Node))); 
		newNode->set(key, val, nullptr, get_rand_level(id));
        
		for (int i = 0; i < newNode->toplevel; i++) {
                    newNode->next[i] = succs[i];
                }
		FLUSH(newNode);

        	/* Node is visible once inserted at lowest level */
        	Node *before = static_cast<Node *>(getCleanReference(succs[0]));
        	if (!preds[0]->CASNext(before, newNode, 0)) {
                    ssmem_free(alloc, newNode);
                    goto retry;
        	}
        	for (int i = 1; i < newNode->toplevel; i++) {
                    while (true) {
                        pred = preds[i];
                        succ = succs[i];
                        //someone has already removed that node
                        if (isMarked(newNode->getNextF(i))) {
                            SFENCE();
                            return true;
                        }
                        if (pred->CASNext(succ, newNode, i))
                            break;
                        search(key, preds, succs);
                    }
                }
                SFENCE();
       		return true;
	}


  private:
	Node *head;

	bool search(int key, Node **left_list, Node **right_list) {
            Node *left_parent, *left, *left_next, *right = nullptr, *right_next;
            retry:
	        left_parent = this->head;
        	left = this->head;
		int num_nodes = 0;
        	for (int i = levelmax - 1; i >= 0; i--) {	
                    left_next = left->getNext(i);

                    if (isMarked(left_next)) {
                        goto retry;
                    }
                    /* Find unmarked node pair at this level - left and right */
                    for (right = left_next;; right = right_next) {
                        /* Skip a sequence of marked nodes */
                        right_next = right->getNext(i);
                	while (isMarked(right_next)) {
                            right = static_cast<Node *>(getCleanReference(right_next));
                            right_next = right->getNext(i);
			    if (i == 0) {
		  	        nodes[num_nodes++] = right;
			    }
                        }
                        if (right->key >= key) {
                            break;
                        }
			left_parent = left;
                        left = right;
                        left_next = right_next;
			num_nodes = 0;
                    }
		    if (i == 0) {
                        nodes[num_nodes++] = left_parent;
                        for (int j = 0; j < num_nodes; j++) {
                           FLUSH(nodes[j]);
                        }
                    }
		    bool cas = false;
                    /* Ensure left and right nodes are adjacent */
		   if (left_next != right) {
                        bool cas = left->CASNext(left_next, right, i);
                        if (!cas) {
                            goto retry;
                        }
                    }
                    if (i == 0 && cas) {
                        for (int j = 0; j < num_nodes-2; j++) {
                           ssmem_free(alloc, nodes[j]);
                        }
                    }
                    if (left_list != nullptr) {
                        left_list[i] = left;
                    }
                    if (right_list != nullptr) {
                        right_list[i] = right;
                    }
                }
                return (right->key == key);
        }
	bool search_no_cleanup(int key, Node **left_list, Node **right_list) {
            Node *left, *left_next, *right = nullptr;
            left = this->head;
            for (int i = levelmax - 1; i >= 0; i--) {
                left_next = static_cast<Node *>(getCleanReference(left->getNext(i)));
                right = left_next;
                while (true) {
                    if (!isMarked(right->getNext(i))) {
                        if (right->key >= key) {
                            break;
                        }
                        left = right;
                    }
                    right = static_cast<Node *>(getCleanReference(right->getNext(i)));
                }

                if (left_list != nullptr) {
                    left_list[i] = left;
                } 
                if (right_list != nullptr) {
                    right_list[i] = right;
                }
            }  
            return (right->key == key);
        }
	bool search_no_cleanup_succs(int key, Node **right_list) {
            Node *left, *left_next, *right = nullptr;
            left = this->head;
            for (int i = levelmax - 1; i >= 0; i--) {
                left_next = static_cast<Node *>(getCleanReference(left->getNext(i)));
                right = left_next;
                while (true) {
                    if (!isMarked(right->getNext(i))) {
                        if (right->key >= key) {
                            break;
                        }
                        left = right;
                    }
                    right = static_cast<Node *>(getCleanReference(right->getNext(i)));
                }
                right_list[i] = right;
            }
            return (right->key == key);
        }
	Node* left_search(int key) {
            Node *left = nullptr, *left_prev;
            left_prev = this->head;
            for (int lvl = levelmax - 1; lvl >= 0; lvl--) {
                left = static_cast<Node *>(getCleanReference(left_prev->getNext(lvl)));
                while (left->key < key || isMarked(left->getNext(lvl))) {
                    if (!isMarked(left->getNext(lvl))) {
                        left_prev = left;
                    }
                    left = static_cast<Node *>(getCleanReference(left->getNext(lvl)));
                }
                if ((left->key == key)) {
                    return left;
                }
            }
            return left_prev;
        }
	inline bool mark_node_ptrs(Node *n) {
            bool cas = false;
            Node *n_next;
            for (int i = n->toplevel - 1; i >= 0; i--) {
                do {
                    n_next = n->getNextF(i);
                    if (isMarked(n_next)) {
                        cas = false;
                        break;
                    }
                    Node *before = static_cast<Node *>(getCleanReference(n_next));
                    Node *after = static_cast<Node *>(getMarkedReference(n_next));
                    cas = n->CASNext(before, after, i);
                } while (!cas);
            }
            return (cas); /* if I was the one that marked lvl 0 */
        }
	int get_rand_level(int seed) {
            int level = 1;
            for (int i = 0; i < levelmax - 1; i++) {
                if ((rand_r_32((unsigned int *)&seed) % 101) < 50)
                        level++;
                else
                        break;
            }

            return level;
        }
} __attribute__((aligned((64))));




#endif /* _SKIPLIST_TRAVERSE_H_ */

