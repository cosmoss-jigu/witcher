#ifndef LIST_IZ_H_
#define LIST_IZ_H_

#include "../Utilities.h"
#include <assert.h>

extern __thread void* nodes[1024];

template <class T> class ListIz{
	public:

		class Node{
			public:
                int key;
				T value;
				Node* volatile next;
//                BYTE padding[CACHE_LINE_SIZE - sizeof(int) - sizeof(T) - sizeof(void*)];

                Node(int k, T val) {
                    key = k;
                    value = val;
                    next = NULL;
                }
            
                Node(int k, T val, Node* n) {
                    key = k;
                    value = val;
                    next = n;
                }
                Node() {
                    key = INT_MIN;
                    value = T();
                    next = NULL;
                }
                void set(int k, T val, Node* n) {
                    key = k;
                    value = val;
                    next = n;
                }
                bool CAS_next(Node* exp, Node* n) {
                    Node* old = next;
                    if (exp != old) {
                        BARRIER(&next);
                        return false;
                    }
                    bool ret = CAS(&next, exp, n);
                    BARRIER(&next);
                    return ret;
                }
                Node* getNext() {
                    Node* n = next;
                    BARRIER(&next);
                    return n;
                }
		};

//===========================================

		class Window {
			public:
				Node* pred;
				Node* curr;
				Window(Node* myPred, Node* myCurr) {
					pred = myPred;
					curr = myCurr;
				}
        };

//===========================================

		ListIz() {
			        //head = static_cast<Node*>(ssmem_alloc(alloc, sizeof(Node)));
head = new Node();
			head->set(INT_MIN, INT_MIN, NULL);
		}
    
        Node* getAdd(Node* n) {
            long node = (long)n;
            return (Node*)(node & ~(0x1L));
        }
    
        bool getMark(Node* n) {
            long node = (long)n;
            return (bool)(node & 0x1L);
        }
    
        Node* mark(Node* n) {
            long node = (long)n;
            node |= 0x1L;
            return (Node*)node;
        }



//===========================================

		Window* find(Node* head, int key) {
            Node* left = head;
            Node* leftNext = head->getNext();
            Node* right = NULL;
            
            Node* curr = NULL;
            Node* currAdd = NULL;
            Node* succ = NULL;
            bool marked = false;
            int numNodes = 0;
            while (true) {
                numNodes = 0;
                curr = head;
                currAdd = curr;
                succ = currAdd->getNext();
                marked = getMark(succ);
                /* 1: Find left and right */
                while (marked || currAdd->key < key) {
                    if (!marked) {
                        left = currAdd;
                        leftNext = succ;
                        numNodes = 0;
                    }
                    nodes[numNodes++] = currAdd;
                    curr = succ;
                    currAdd = getAdd(curr);
                    if (currAdd == NULL) {
                        break;
                    }
                    succ = currAdd->getNext();                  // load_acq
                    marked = getMark(succ);
                }
                right = currAdd;
                /* 2: Check nodes are adjacent */
                if (leftNext == right) {
                    if ((right != NULL) && getMark(right->getNext())) {
                        continue;
                    } else {
                        Window* w = static_cast<Window*>(ssmem_alloc(allocW, sizeof(Window)));
                        w->pred = left;
                        w->curr = right;
                        return w;
                    }
                }
                /* 3: Remove one or more marked nodes */
                if (left->CAS_next(leftNext, right)) {
                    for (int i = 1; i < numNodes; i++) {
                        if (nodes[i]) {
                            ssmem_free(alloc, nodes[i]);
                            //EpochReclaimObject(epoch, (Node*)nodes[i], NULL, NULL, finalize_node_list);
                        }
                    }
                    if ((right != NULL) && getMark(right->getNext())) {
                        continue;
                    } else {
                        Window* w = static_cast<Window*>(ssmem_alloc(allocW, sizeof(Window)));
                        w->pred = left;
                        w->curr = right;
                        return w;
                    }
                }
            }
        }
            
    

//=========================================


		bool insert(int k, T item) {
			while (true) {
				Window* window = find(head, k);
                Node* curr = window->curr;
                Node* pred = window->pred;
                ssmem_free(allocW, window);
            	if (curr && curr->key == k) {
                	return false;
            	}
            	Node* node = static_cast<Node*>(ssmem_alloc(alloc, sizeof(Node)));
		node->set(k, item, curr); //store
                FLUSH(node);
                if (pred->CAS_next(curr, node)) {// store_rel
                    SFENCE();
                    return true;
                } else {
                	ssmem_free(alloc, node);
		}
		}
		}

//========================================

		bool remove(int k) {
			int key = k;
			bool snip;
			while (true) {
				Window* window = find(head, key);
				Node* pred = window->pred;
				Node* curr = window->curr;
				            ssmem_free(allocW, window);
				if (!curr || curr->key != key) { //load - immutable
					SFENCE();
					return false;
				} else {
                    Node* succ = curr->getNext();
                    Node* succAndMark = mark(succ);
                    if (succ == succAndMark) {
                        continue;
                    }
                    snip = curr->CAS_next(succ, succAndMark);
                    if (!snip)
                        continue;
					if (pred->CAS_next(curr, succ)) {
					    ssmem_free(alloc, curr);

					}
					SFENCE();
                    return true;
				}
			}
		}
    
    //========================================
    
    		bool contains(int k) {
                int key = k;
                Node* curr = head;
                bool marked = getMark(curr->getNext());
                while (curr->key < key) {
                    curr = getAdd(curr->getNext());
                    if (!curr) {
                        SFENCE();
                        return false;
                    }
                    marked = getMark(curr->getNext());
                }
        		if (curr->key == key && !marked){
                    SFENCE();
                    return true;
        		} else {
                    SFENCE();
                    return false;
        		}
    		}

//========================================

	private:
		Node* volatile head;

};
#endif /* LIST_IZ_H_ */

