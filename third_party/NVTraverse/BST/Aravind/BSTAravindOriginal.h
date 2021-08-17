/*   
 *   File: bst-aravind.h
 *   Author: Tudor David <tudor.david@epfl.ch>
 *   Description: Aravind Natarajan and Neeraj Mittal. 
 *   Fast Concurrent Lock-free Binary Search Trees. PPoPP 2014
 *   bst-aravind.h is part of ASCYLIB
 *
 * Copyright (c) 2014 Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>,
 * 	     	      Tudor David <tudor.david@epfl.ch>
 *	      	      Distributed Programming Lab (LPD), EPFL
 *
 * ASCYLIB is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#ifndef _BST_ARAVIND_ORIGINAL_H_
#define _BST_ARAVIND_ORIGINAL_H_

#include <stdio.h>
#include <stdlib.h>
#include "../utils.h"
#include "../new_pmem_utils.h"
#include "../../gc/ssmem.h"

extern __thread ssmem_allocator_t* alloc;
extern __thread ssmem_allocator_t* allocW;
__thread seek_record_t* seek_record;

#define GC 1

//RETRY_STATS_VARS;

template <class T> class BSTAravindOriginal {
    public:
	BSTAravindOriginal() {root = initialize_tree();}
	
	node_t* initialize_tree() {
	    node_t* r;
            node_t* s;
    	    node_t* inf0;
    	    node_t* inf1;
    	    node_t* inf2;
    	    r = create_node_initial(DUMMY_TID, INF2,0,1);
    	    s = create_node_initial(DUMMY_TID, INF1,0,1);
    	    inf0 = create_node_initial(DUMMY_TID, INF0,0,1);
    	    inf1 = create_node_initial(DUMMY_TID, INF1,0,1);
    	    inf2 = create_node_initial(DUMMY_TID, INF2,0,1);

    	    asm volatile("" ::: "memory");
    	    r->left = s;
    	    r->right = inf2;
    	    s->right = inf1;
    	    s->left= inf0;
    	    asm volatile("" ::: "memory");

    	    return r;
	}
//============================================
	
	void bst_init_local() {
	    seek_record = (seek_record_t*) malloc (sizeof(seek_record_t));
  	    assert(seek_record != NULL);
	}
//===========================================
	
	node_t* create_node(const int tid, skey_t k, sval_t value, int initializing) {
	    node_t* new_node;
	    #if GC == 1
                new_node = (node_t*) ssmem_alloc(alloc, sizeof(node_t));
	    #else
    		new_node = (node_t*) malloc(sizeof(node_t));
	    #endif
   	    if (new_node == NULL) {
            	perror("malloc in bst create node");
            	exit(1);
            }
    	    new_node->left = (node_t*) NULL;
    	    new_node->right = (node_t*) NULL;
    	    new_node->key = k;
    	    new_node->value = value;
	    asm volatile("" ::: "memory");
    	    return (node_t*) new_node;
	}

//===========================================

        node_t* create_node_initial(const int tid, skey_t k, sval_t value, int initializing) {
            node_t* new_node;
            new_node = (node_t*) malloc(sizeof(node_t));
            if (new_node == NULL) {
                perror("malloc in bst create node");
                exit(1);
            }
            new_node->left = (node_t*) NULL;
            new_node->right = (node_t*) NULL;
            new_node->key = k;
            new_node->value = value;
            asm volatile("" ::: "memory");
	    return (node_t*) new_node;
        }

//===========================================

	seek_record_t * bst_seek(const int tid, skey_t key, node_t* node_r) {
	    //PARSE_TRY();
	    seek_record_t seek_record_l;
    	    node_t* node_s = ADDRESS(node_r->left); // root is immutable
    	    seek_record_l.ancestor = node_r; // not a shared variable
    	    seek_record_l.successor = node_s;
    	    seek_record_l.parent = node_s;
    	    seek_record_l.leaf = ADDRESS(node_s->left);

    	    node_t* parent_field = (node_t*) iz::load_acq(tid, seek_record_l.parent->left);
    	    node_t* current_field = (node_t*) iz::load_acq(tid, seek_record_l.leaf->left);
    	    node_t* current = ADDRESS(current_field);

    	    while (current != NULL) {
        	if (!GETTAG(parent_field)) {
		    seek_record_l.ancestor = seek_record_l.parent;
            	    seek_record_l.successor = seek_record_l.leaf;
        	}
        	seek_record_l.parent = seek_record_l.leaf;
        	seek_record_l.leaf = current;

        	parent_field = current_field;
        	if (key < current->key) {
            	    current_field = (node_t*) current->left;
        	} else {
            	    current_field = (node_t*) current->right;
        	}
        	current=ADDRESS(current_field);
    	    }
    	    seek_record->ancestor=seek_record_l.ancestor;
    	    seek_record->successor=seek_record_l.successor;
    	    seek_record->parent=seek_record_l.parent;
    	    seek_record->leaf=seek_record_l.leaf;
    	    return seek_record;
	}

//===================================================

	sval_t contains(const int tid, skey_t key) {
   	    bst_seek(tid, key, root);
   	    if (seek_record->leaf->key == key) {
       		sval_t ret = seek_record->leaf->value;
       	    	return ret;
   	    } else {
       		return 0;
   	    }
	}
	
//====================================================	
	bool_t insert(const int tid, skey_t key, sval_t val) {
    	    node_t* new_internal = NULL;
    	    node_t* new_node = NULL;
    	    uint created = 0;
    	    while (1) {
      		//UPDATE_TRY();
        	bst_seek(tid, key, root);

		if (seek_record->leaf->key == key) {
		    #if GC == 1
            	    	if (created) {
                	    ssmem_free(alloc, new_internal);
                	    ssmem_free(alloc, new_node);
            	    	}
		    #endif
            	    return FALSE;
            	}

            	node_t* parent = seek_record->parent;
            	node_t* leaf = seek_record->leaf;

            	node_t** child_addr;
            	if (key < parent->key) {
          	    child_addr= (node_t**) &(parent->left);
            	} else {
            	    child_addr= (node_t**) &(parent->right);
            	}
            	if (/*likely*/(created==0)) {
            	    new_internal=create_node(tid, max(key,leaf->key),0,0);
            	    new_node = create_node(tid, key,val,0);
            	    created=1;
           	 } else {
            	    iz::store(tid, new_internal->key, max(key,leaf->key));
            	}
            	if ( key < leaf->key) {
            	    new_internal->left = new_node;
            	    new_internal->right = leaf;
            	} else {
            	    new_internal->right = new_node;
            	    new_internal->left = leaf;
            	}
	        #ifdef __tile__
    		    MEM_BARRIER;
	    	#endif
            	node_t* result = CASV(child_addr, ADDRESS(leaf), ADDRESS(new_internal));
            	if (result == ADDRESS(leaf)) {
            	    iz::end_operation(tid);
            	    return TRUE;
            	}
            	node_t* chld = *child_addr;
            	if ( (ADDRESS(chld)==leaf) && (GETFLAG(chld) || GETTAG(chld)) ) {
            	    bst_cleanup(tid, key);
            	}
    	    }
    	    iz::end_operation(tid);
    	}

//=====================================================

	sval_t remove(const int tid, skey_t key) {
    	    bool_t injecting = TRUE;
    	    node_t* leaf;
    	    sval_t val = 0;
    	    while (1) {
      	        //UPDATE_TRY();
        	bst_seek(tid, key, root);
        	val = seek_record->leaf->value;
        	node_t* parent = seek_record->parent;

        	node_t** child_addr;
        	if (key < parent->key) {
            	    child_addr = (node_t**) &(parent->left);
        	} else {
            	    child_addr = (node_t**) &(parent->right);
        	}

        	if (injecting == TRUE) {
            	    leaf = seek_record->leaf;
            	    if (leaf->key != key) {
                	return 0;
            	    }
            	    node_t* lf = ADDRESS(leaf);
            	    node_t* result = CASV(child_addr, lf, (node_t*) FLAG(lf));
            	    if (result == ADDRESS(leaf)) {
                	injecting = FALSE;
                	bool_t done = bst_cleanup(tid, key);
                	if (done == TRUE) {
                    	    return val;
                	}
            	    } else {
                	node_t* chld = *child_addr;
                	if ( (ADDRESS(chld) == leaf) && (GETFLAG(chld) || GETTAG(chld)) ) {
                    	    bst_cleanup(tid, key);
                	}
            	    }
        	} else {
            	    if (seek_record->leaf != leaf) {
                	return val;
            	    } else {
                	bool_t done = bst_cleanup(tid, key);
                	if (done == TRUE) {
                    	    return val;
                	}
            	    }
        	}
    	    }
	}


//=====================================================

	bool_t bst_cleanup(const int tid, skey_t key) {
	    node_t* ancestor = seek_record->ancestor;
    	    node_t* successor = seek_record->successor;
    	    node_t* parent = seek_record->parent;

    	    node_t** succ_addr;
    	    if (key < ancestor->key) {
        	succ_addr = (node_t**) &(ancestor->left);
    	    } else {
        	succ_addr = (node_t**) &(ancestor->right);
    	    }

    	    node_t** child_addr;
    	    node_t** sibling_addr;
    	    if (key < parent->key) {
       		child_addr = (node_t**) &(parent->left);
       		sibling_addr = (node_t**) &(parent->right);
    	    } else {
       		child_addr = (node_t**) &(parent->right);
       		sibling_addr = (node_t**) &(parent->left);
    	    }

    	    node_t* chld = *(child_addr);
    	    if (!GETFLAG(chld)) {
        	chld = *(sibling_addr);
        	sibling_addr = child_addr;
    	    }
    	    while (1) {
        	node_t* untagged = *sibling_addr;
        	node_t* tagged = (node_t*)TAG(untagged);
        	node_t* res = CASV(sibling_addr, untagged, tagged);
        	if (res == untagged) {
            	    break;
         	}
    	    }

	    node_t* sibl = *sibling_addr;
    	    if (CASV(succ_addr, ADDRESS(successor), (node_t*) UNTAG(sibl)) == (node_t*) ADDRESS(successor)) {
	    	#if GC == 1
    		    ssmem_free(alloc, ADDRESS(chld));
		#endif
        	return TRUE;
    	    }
    	    return FALSE;
	}

//=====================================================

	uint32_t bst_size(volatile node_t* node) {
    	    if (node == NULL) return 0;
    	    if ((node->left == NULL) && (node->right == NULL)) {
       	    	if (node->key < INF0 ) return 1;
    	    }
    	    uint32_t l = 0;
    	    uint32_t r = 0;
    	    if ( !GETFLAG(node->left) && !GETTAG(node->left)) {
        	l = bst_size(node->left);
    	    }
    	    if ( !GETFLAG(node->right) && !GETTAG(node->right)) {
        	r = bst_size(node->right);
    	    }
    	    return l+r;
	}

    private:
        node_t* root;
     
};
#endif

