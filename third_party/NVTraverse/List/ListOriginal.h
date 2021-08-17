#ifndef LIST_ORIGINAL_H_
#define LIST_ORIGINAL_H_

#include "../Utilities.h"
#include <assert.h>

template <class T> class ListOriginal{
public:
    class Node{
    public:
        int key;
        T value;
        Node* volatile next;
       // BYTE padding[CACHE_LINE_SIZE - sizeof(int) - sizeof(T) - sizeof(void*)];

        Node(int k, T val) {
            key = k;
            value = val;
            next = NULL;
        }
        Node() {
            key = INT_MIN;
            value = T();
            next = NULL;
        }
        Node(int k, T val, Node* n) {
            key = k;
            value = val;
            next = n;
        }
        Node* getNextF() {
            Node* n = next;
            return n;
        }
        Node* getNext() {
            return next;
        }
        bool CAS_nextF(Node* exp, Node* n) {
            bool ret = CAS(&next, exp, n);
            return ret;
        }
        
        bool CAS_next(Node* exp, Node* n) {
            Node* old = next;
            if(exp != old) return false;
            bool ret = CAS(&next, old, n);
            return ret;
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

    ListOriginal() {
	    //cout << "in constructor" << endl;
	    //cout << alloc->ts->id << endl;
//	head = static_cast<Node*>(ssmem_alloc(alloc, sizeof(Node)));

        head = new Node(INT_MIN, INT_MIN);
        head->next = NULL;
    }
    
    Node* getAdd(Node* n) {
        long node = (long)n;
            return (Node*)(node & ~(0x1L)); // clear bit to get the real address
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
                    currAdd = getAdd(curr);        // load_acq
                    if (currAdd == NULL) {
                        break;
                    }
                    succ = currAdd->getNext();
                    marked = getMark(succ);
                }
                right = currAdd;
                /* 2: Check nodes are adjacent */
                if (leftNext == right) {
                   if ((right != NULL) && getMark(right->getNext())) {
		  //  cout << "problem1" << endl;
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
		//	cout << "before free in find" << endl;
			ssmem_free(alloc, nodes[i]);
                        //EpochReclaimObject(epoch, (Node*)nodes[i], NULL, NULL, finalize_node_list);
                    }
                }
		if ((right != NULL) && getMark(right->getNext())) {
		 //   cout << "problem2" << endl;
                    continue;
                } else {
		    Window* w = static_cast<Window*>(ssmem_alloc(allocW,
					    sizeof(Window)));
                    w->pred = left;
                    w->curr = right;
                    return w;
                }
            }
        }
    }

//=========================================

    bool insert(int k, T item) {
	   // return true;
        //bool add(T item, int k, int threadID) {
        while (true) {
            Window* window = find(head, k);
            Node* pred = window->pred;
            Node* curr = window->curr;
	    ssmem_free(allocW, window);
            if (curr && curr->key == k) {
                return false;
            }
            Node* node = static_cast<Node*>(ssmem_alloc(alloc, sizeof(Node)));
 	    node->key = k;
	    node->value = item;
	    node->next = curr;
            bool res = pred->CAS_nextF(curr, node);
            if (res) {
                return true;
            } else {
		ssmem_free(alloc, node);
                continue;
            }
        }
    }

//========================================

    bool remove(int key) {
	//cout << "in remove" << endl;
        bool snip = false;
        while (true) {
            Window* window = find(head, key);
	    //cout << "after find" << endl;
            Node* pred = window->pred;
            Node* curr = window->curr;
	    //            cout << "before free allocW" << endl;
	    ssmem_free(allocW, window);
	    //cout << "after free allocW" << endl;
            if (!curr || curr->key != key) {
	    //	 cout << "return false" << endl;
                return false;
            } else {
                Node* succ = curr->next;
                Node* succAndMark = mark(succ);
                if (succ == succAndMark) {
		    //cout << "in succAndMark" << endl;
                    continue;
                }
		//cout << "before CAS" << endl;
                snip = curr->CAS_next(succ, succAndMark);
                //                cout << "after CAS" << endl;
		//		cout << snip << endl;
		if (!snip) {
		 //   cout << "in not able to mark" << endl;
                    continue;
		}
		   //             cout << "before CAS2" << endl;
                if(pred->CAS_next(curr, succ)){           //succ is not marked
                //	cout << "before free" << endl;	
			ssmem_free(alloc, curr);
		}
	//	                 cout << "return true" << endl;
		return true;
            }
        }
    }

//========================================

        bool contains(int k) {
            //return true;
		int key = k;
            Node* curr = head;
            bool marked = getMark(curr->next);
            while (curr && curr->key < key) {
//		cout << curr->key << endl;
                curr = getAdd(curr->next);
                if (!curr) {
//			                                 cout << "return false in contains" << endl;
                    return false;
                }
                marked = getMark(curr->next);
            }
            if(curr->key == key && !marked){
//		                                                             cout << "return true in contains" << endl;
                return true;
            } else {
//		                                                             cout << "return false in contains" << endl;

                return false;
            }
        }

//========================================

    private:
        Node* head;

    };

#endif /* LIST_ORIGINAL_H_ */


