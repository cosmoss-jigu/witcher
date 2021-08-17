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


#ifndef _BST_ARAVIND_IZ_H_
#define _BST_ARAVIND_IZ_H_

#include <stdio.h>
#include <stdlib.h>
#include "../utils.h"
#include "../new_pmem_utils.h"
#include "../../gc/ssmem.h"

extern __thread ssmem_allocator_t* alloc;
extern __thread ssmem_allocator_t* allocW;
__thread seek_record_t* seek_record_iz;

#define GC 1

namespace iz = izrealivitz_transformation;

//RETRY_STATS_VARS;

template <class T> class BSTAravindIz {
    public:
	BSTAravindIz() {root = initialize_tree();}
	
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
	    seek_record_iz = (seek_record_t*) malloc (sizeof(seek_record_t));
  	    assert(seek_record_iz != NULL);
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
    	    iz::store(tid, new_node->left, (node_t*) NULL);
    	    iz::store(tid, new_node->right, (node_t*) NULL);
    	    iz::store(tid, new_node->key, k);
    	    iz::store(tid, new_node->value, value);
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
            iz::store(tid, new_node->left, (node_t*) NULL);
            iz::store(tid, new_node->right, (node_t*) NULL);
            iz::store(tid, new_node->key, k);
            iz::store(tid, new_node->value, value);
            return (node_t*) new_node;
        }

//===========================================

	seek_record_t * bst_seek(const int tid, skey_t key, node_t* node_r) {
	    //PARSE_TRY();
	    seek_record_t seek_record_l;
    	    node_t* node_s = ADDRESS(iz::load(tid, node_r->left)); // root is immutable
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
        	if (key < iz::load(tid, current->key)) {
            	    current_field= (node_t*) iz::load_acq(tid, current->left);
        	} else {
            	    current_field= (node_t*) iz::load_acq(tid, current->right);
        	}
        	current=ADDRESS(current_field);
    	    }
    	    seek_record_iz->ancestor=seek_record_l.ancestor;
    	    seek_record_iz->successor=seek_record_l.successor;
    	    seek_record_iz->parent=seek_record_l.parent;
    	    seek_record_iz->leaf=seek_record_l.leaf;
    	    return seek_record_iz;
	}

//=================================================
	/*
    	    frees all nodes in the interval [node_start, node_stop]
	*/
	void free_path(const int tid, skey_t key, node_t* node_start, node_t* node_stop) {
    	    node_t* current = node_start;
    	    node_t* current_field = NULL;

   	     while (current != node_stop) {
        	if(current == NULL) return; // this is a safety check, current should never be NULL
            	ssmem_free(alloc, current);
        	if (key < current->key) {
            	    current_field= (node_t*) current->left;
        	} else {
            	    current_field= (node_t*) current->right;
        	}
        	current=ADDRESS(current_field);
    	    }
    	    ssmem_free(alloc, current);
	}

//===================================================

	sval_t contains(const int tid, skey_t key) {
   	    bst_seek(tid, key, root);
   	    if (iz::load(tid, seek_record_iz->leaf->key) == key) {
       		sval_t ret = iz::load(tid, seek_record_iz->leaf->value);
       		iz::end_operation(tid);
       	    	return ret;
   	    } else {
       		iz::end_operation(tid);
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

		if (iz::load(tid, seek_record_iz->leaf->key) == key) {
		    #if GC == 1
            	    	if (created) {
                	    ssmem_free(alloc, new_internal);
                	    ssmem_free(alloc, new_node);
            	    	}
		    #endif
            	    iz::end_operation(tid);
            	    return FALSE;
            	}

            	node_t* parent = seek_record_iz->parent;
            	node_t* leaf = seek_record_iz->leaf;

            	node_t** child_addr;
            	if (key < iz::load(tid, parent->key)) {
          	    child_addr= (node_t**) &(parent->left);
            	} else {
            	    child_addr= (node_t**) &(parent->right);
            	}
            	if (/*likely*/(created==0)) {
            	    new_internal=create_node(tid, max(key,iz::load(tid, leaf->key)),0,0);
            	    new_node = create_node(tid, key,val,0);
            	    created=1;
           	 } else {
            	    iz::store(tid, new_internal->key, max(key,iz::load(tid, leaf->key)));
            	}
            	if ( key < iz::load(tid, leaf->key)) {
            	    iz::store(tid, new_internal->left, new_node);
            	    iz::store(tid, new_internal->right, leaf);
            	} else {
            	    iz::store(tid, new_internal->right, new_node);
            	    iz::store(tid, new_internal->left, leaf);
            	}
	        #ifdef __tile__
    		    MEM_BARRIER;
	    	#endif
            	node_t* result = iz::casv(tid, child_addr, ADDRESS(leaf), ADDRESS(new_internal));
            	if (result == ADDRESS(leaf)) {
            	    iz::end_operation(tid);
            	    return TRUE;
            	}
            	node_t* chld = iz::load_acq(tid, *child_addr);
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
        	val = iz::load(tid, seek_record_iz->leaf->value);
        	node_t* parent = seek_record_iz->parent;

        	node_t** child_addr;
        	if (key < iz::load(tid, parent->key)) {
            	    child_addr = (node_t**) &(parent->left);
        	} else {
            	    child_addr = (node_t**) &(parent->right);
        	}

        	if (injecting == TRUE) {
            	    leaf = seek_record_iz->leaf;
            	    if (iz::load(tid, leaf->key) != key) {
                	iz::end_operation(tid);
                	return 0;
            	    }
            	    node_t* lf = ADDRESS(leaf);
            	    node_t* result = iz::casv(tid, child_addr, lf, (node_t*) FLAG(lf));
            	    if (result == ADDRESS(leaf)) {
                	injecting = FALSE;
                	bool_t done = bst_cleanup(tid, key);
                	if (done == TRUE) {
                    	    iz::end_operation(tid);
                    	    return val;
                	}
            	    } else {
                	node_t* chld = iz::load_acq(tid, *child_addr);
                	if ( (ADDRESS(chld) == leaf) && (GETFLAG(chld) || GETTAG(chld)) ) {
                    	    bst_cleanup(tid, key);
                	}
            	    }
        	} else {
            	    if (seek_record_iz->leaf != leaf) {
                	iz::end_operation(tid);
                	return val;
            	    } else {
                	bool_t done = bst_cleanup(tid, key);
                	if (done == TRUE) {
                    	    iz::end_operation(tid);
                    	    return val;
                	}
            	    }
        	}
    	    }
    	    iz::end_operation(tid);
	}


//=====================================================

	bool_t bst_cleanup(const int tid, skey_t key) {
	    node_t* ancestor = seek_record_iz->ancestor;
    	    node_t* successor = seek_record_iz->successor;
    	    node_t* parent = seek_record_iz->parent;

    	    node_t** succ_addr;
    	    if (key < iz::load(tid, ancestor->key)) {
        	succ_addr = (node_t**) &(ancestor->left);
    	    } else {
        	succ_addr = (node_t**) &(ancestor->right);
    	    }

    	    node_t** child_addr;
    	    node_t** sibling_addr;
    	    if (key < iz::load(tid, parent->key)) {
       		child_addr = (node_t**) &(parent->left);
       		sibling_addr = (node_t**) &(parent->right);
    	    } else {
       		child_addr = (node_t**) &(parent->right);
       		sibling_addr = (node_t**) &(parent->left);
    	    }

    	    node_t* chld = iz::load_acq(tid, *(child_addr));
    	    if (!GETFLAG(chld)) {
        	chld = iz::load_acq(tid, *(sibling_addr));
        	sibling_addr = child_addr;
    	    }
    	    while (1) {
        	node_t* untagged = iz::load_acq(tid, *sibling_addr);
        	node_t* tagged = (node_t*)TAG(untagged);
        	node_t* res = iz::casv(tid, sibling_addr, untagged, tagged);
        	if (res == untagged) {
            	    break;
         	}
    	    }

	    node_t* sibl = iz::load_acq(tid, *sibling_addr);
    	    if (iz::casv(tid, succ_addr, ADDRESS(successor), (node_t*) UNTAG(sibl)) == (node_t*) ADDRESS(successor)) {
	    	#if GC == 1
    		    ssmem_free(alloc, ADDRESS(chld));
    		    free_path(tid, parent->key, ADDRESS(successor), parent);
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

