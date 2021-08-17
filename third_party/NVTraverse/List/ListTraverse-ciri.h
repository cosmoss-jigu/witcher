#ifndef LIST_TRAVERSE_H_
#define LIST_TRAVERSE_H_

#include "Utilities.h"
#include "pmdk.h"
#include <assert.h>


extern __thread void* nodes[1024];

template <class T> class ListTraverse {	
	public:

	class Node{
	public:
		T value;
		int key;
		Node* volatile next;
    		//BYTE padding[CACHE_LINE_SIZE - sizeof(int) - sizeof(T) - sizeof(void*)];

		Node(T val, int k, Node* n) {
			value = val;
			key = k;
			next = n;
		}

		Node(T val, int k) {
			value = val;
			key = k;
			next = NULL;
		}
		Node() {
			value = T();
			key = 0;
			next = NULL;
		}
        void set(int k, T val, Node* n) {
            key = k;
            value = val;
            next = n;
        }
        Node* getNextF() {
            Node* n = next;
            FLUSH(&next);
            return n;
        }
        Node* getNext() {
            Node* n = next;
            return n;
        }
        bool CAS_nextF(Node* exp, Node* n) {
            Node* old = next;
            if(exp != old) {
                FLUSH(&next);
                return false;
            }
            bool ret = CAS(&next, old, n);
            FLUSH(&next);
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
  void* operator new(size_t size) {
    void* ret = nvm_alloc(size);
    return ret;
  }

	ListTraverse() {
    // head = static_cast<Node*>(ssmem_alloc(alloc, sizeof(Node)));//(Node*)EpochAllocNode(epoch, sizeof(Node));
    // head = new Node();
    head = (Node*) nvm_alloc(sizeof(Node));
		head->set(INT_MIN, INT_MIN, NULL);
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

    	Window* find(Node* head, int key/*, EpochThread epoch*/) {
            Node* leftParent = head;
            Node* left = head;
            Node* leftNext = head->getNext();
            Node* right = NULL;
            
            Node* pred = NULL;
            Node* curr = NULL;
            Node* currAdd = NULL;
            Node* succ = NULL;
            bool marked = false;
            int numNodes = 0;
            while (true) {
                numNodes = 0;
                pred = head;
                curr = head;
                currAdd = curr;
                succ = currAdd->getNext();
                marked = getMark(succ);
                /* 1: Find left and right */
                while (marked || currAdd->key < key) {
                    if (!marked) {
                        leftParent = pred;
                        left = currAdd;
                        leftNext = succ;
                        numNodes = 0;
                        //nodes[numNodes++] = leftNext;
                    }
                    nodes[numNodes++] = currAdd;
                    pred = currAdd;
                    curr = succ;
                    currAdd = getAdd(curr);        // load_acq
                    if (currAdd == NULL) {
                        break;
                    }
                    succ = currAdd->getNext();
                    marked = getMark(succ);
                }
                right = currAdd;
                nodes[numNodes++] = right;
                /* 2: Check nodes are adjacent */
                if (leftNext == right) {
                    if ((right != NULL) && getMark(right->getNext())) {
                        continue;
                    } else {
                        nodes[numNodes++] = leftParent;
                        for (int i = 0; i < numNodes; i++) {
                            if (nodes[i]) FLUSH(nodes[i]);
                        }
			                  //Window* w = static_cast<Window*>(ssmem_alloc(allocW, sizeof(Window)));
                        //w->pred = left;
                        //w->curr = right;
                        //return w;
                        return new Window(left, right);
                    }
                }
                nodes[numNodes++] = leftParent;
                for (int i = 0; i < numNodes; i++) {
                    if (nodes[i]) {
                        //if (i!=0 && i <= numNodes-3) {
                        //    EpochDeclareUnlinkNode(epoch, (void*)nodes[i], sizeof(Node));
                        //}
			FLUSH(nodes[i]);
                    }
                }
                /* 3: Remove one or more marked nodes */
                if (left->CAS_nextF(leftNext, right)) {
                    for (int i = 1; i <= numNodes-3; i++) {
                        if (nodes[i]) {
                            nvm_free(nodes[i]);
                            //ssmem_free(alloc, nodes[i]);	
                            //EpochReclaimObject(epoch, (Node*)nodes[i], NULL, NULL, finalize_node_list_gen);
                        }
                    }
                    if ((right != NULL) && getMark(right->getNextF())) {
                        continue;
                    } else {
                        //Window* w = static_cast<Window*>(ssmem_alloc(allocW, sizeof(Window)));
                        //Window* w = new Window();
                        //w->pred = left;
                        //w->curr = right;
                        //return w;
                        return new Window(left, right);
                    }
                }
            }
        }
    
//=========================================
    
    	bool insert(int k, T item/*, EpochThread epoch*/) {
            //EpochStart(epoch);
            while (true) {
                Window* window = find(head, k/*, epoch*/);
                Node* pred = window->pred;
                Node* curr = window->curr;
                free(window);
                //ssmem_free(allocW, window);
                if (curr && curr->key == k) {
                    //EpochEnd(epoch);
                    SFENCE();
                    return false;
                }
                Node* node = (Node*) nvm_alloc(sizeof(Node));
		            //Node* node = static_cast<Node*>(ssmem_alloc(alloc, sizeof(Node)));//(Node*)EpochAllocNode(epoch, sizeof(Node));
                //Node* node = (Node*)EpochAllocNode(epoch, sizeof(Node));
                node->set(k, item, curr); //store
                FLUSH(node);
                bool res = pred->CAS_nextF(curr, node);
                if (res) {
                    //EpochEnd(epoch);
                    SFENCE();
                    return true;
                }
                nvm_free(node);
                //ssmem_free(alloc, node);
                //finalize_node_list_gen((void*)node, NULL, NULL);
            }
        }


//========================================

    	bool remove(int key/*, EpochThread epoch*/) {
            //EpochStart(epoch);
    		bool snip = false;
    		while (true) {
    			Window* window = find(head, key/*, epoch*/);
    			Node* pred = window->pred;
    			Node* curr = window->curr;
          free(window);
          //ssmem_free(allocW, window);
    			if (!curr || curr->key != key) {
                    //EpochEnd(epoch);
                    SFENCE();
    				return false;
    			} else {
                    Node* succ = curr->getNextF();
                    Node* succAndMark = mark(succ);
                    if (succ == succAndMark) {
                        continue;
                    }
                    snip = curr->CAS_nextF(succ, succAndMark);
    				if (!snip)
    					continue;
                    //EpochDeclareUnlinkNode(epoch, (void*)curr, sizeof(Node));
                    if (pred->CAS_nextF(curr, succ)){
                      nvm_free(curr);
                      //ssmem_free(alloc, curr);
                      //EpochReclaimObject(epoch, (Node*)curr, NULL, NULL, finalize_node_list_gen);
                    }
                    //EpochEnd(epoch);
                    SFENCE();
    		    return true;
    			}
    		}
    	}
    
    //========================================
    
    	T contains(int key/*, EpochThread epoch*/) {
            //EpochStart(epoch);
            Node* pred = head;
            Node* curr = head;
            bool marked = getMark(curr->getNext());
            while (curr->key < key) {
                pred = curr;
                curr = getAdd(curr->getNext());
                if (!curr) {
                    FLUSH(pred);
		    //EpochEnd(epoch);
                    SFENCE();
                    return false;
                }
                marked = getMark(curr->getNext());
            }
            FLUSH(pred);
            FLUSH(curr);
            if(curr->key == key && !marked){
                //EpochEnd(epoch);
                SFENCE();
                //return true;
                return curr->value;
            } else {
                //EpochEnd(epoch);
                SFENCE();
                //return false;
                return NULL;
            }
        }
    
    //========================================
    

private:
	Node* volatile head;

};

ListTraverse<int > *init_list(char *path,
                              size_t size,
                              char *layout_name)
{
    int is_created;
    root_obj *root_obj = init_nvmm_pool(path, size, layout_name, &is_created);
    if (is_created == 0) {
      printf("Reading from an existing List.\n");
      ListTraverse<int>* ret = (ListTraverse<int>*) root_obj->ptr;
      return ret;
    }
    ListTraverse<int>* list = new ListTraverse<int>();

    root_obj->ptr = list;
    FLUSH((char *)root_obj);
    SFENCE();
    return list;
}

#endif /* LIST_TRAVERSE_H_ */


