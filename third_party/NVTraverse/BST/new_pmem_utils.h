#ifndef NEW_PMEM_UTILS_H_
#define NEW_PMEM_UTILS_H_

#include "pmem_utils.h"
#include <iostream>
#include <assert.h>

/* TODOS
	- Finish implementing all the flush marking methods
	- Apply them to Queue with IZ transformation
	- Transform balanced BST
	- Find optimal number of flush counters
	- put volatiel in the right places
	- look into link and persist implementation for stores

	- apply cache line optimization
	- look into why you used write_no_fence before
	- apply link and persist to BST using double word CAS
	- run Rachid's BST
	- transform balanced BST
	- compare OP with KB
	- run on real NVM
	- compare iz with flush marking with 
*/

namespace flush_mark {

	// we assume you do CAS with a value you previously read.
	// This simplifies things in the link and persist case because
	// that value must have been flushed before you reach this method.
	// That argument also assumes that the same value is not writen twice.
	template <class ET>
	inline bool fence_casb_flush_fence(const int tid, ET volatile *ptr, ET oldv, ET newv) { 
		bool ret;
		//implicit_fence(tid);
		ret = CASB(ptr, oldv, newv);
		FLUSH(tid, ptr);
		#ifndef IZ_NO_FENCE_AFTER_LOAD
		FENCE(tid);			
		#endif
		return ret;
	}


	// we assume you do CAS with a value you previously read.
	// This ismplifies things in the link and persist case because
	// that value must have been flushed before you reach this method.
	// That argument also assumes that the same value is not writen twice.
	template <class ET>
	inline bool fence_casb_flush(const int tid, ET volatile *ptr, ET oldv, ET newv) { 
		bool ret;
		//implicit_fence(tid);
		ret = CASB(ptr, oldv, newv);
		FENCE(tid, ptr);
		return ret;
	}	

	// we assume you do CAS with a value you previously read.
	// This ismplifies things in the link and persist case because
	// that value must have been flushed before you reach this method.
	// That argument also assumes that the same value is not writen twice.
	template <class ET>
	inline ET fence_casv_flush_fence(const int tid, ET volatile *ptr, ET oldv, ET newv) { 
		ET ret;
		//implicit_fence(tid);
		ret = CASV(ptr, oldv, newv);
		FLUSH(tid, ptr);
		#ifndef IZ_NO_FENCE_AFTER_LOAD
		FENCE(tid);
		#endif	
		return ret;
	}

	// we assume you do CAS with a value you previously read.
	// This ismplifies things in the link and persist case because
	// that value must have been flushed before you reach this method.
	// That argument also assumes that the same value is not writen twice.
	template <class ET>
	inline ET fence_casv_flush(const int tid, ET volatile *ptr, ET oldv, ET newv) { 
		ET ret;
		//implicit_fence(tid);
		ret = CASV(ptr, oldv, newv);
		FLUSH(tid, ptr);
		return ret;
	}
}

namespace izrealivitz_transformation {

	/* load cannot be concurrent with a store so 
	   in the case of LINK_AND_PERSIST, the flush
	   bit is definiately not set.
	*/
	template <class ET>
	inline ET load(const int tid, ET &ptr) {
		return ptr;
	}

	/* NODE_T is only used if FLUSH_MARK_NODE is defined.
	   If LINK_AND_PERSIST is defined, then we use the second 
	   least significant bit of a value to do marking.
	*/
	template <class ET>
	inline ET load_acq(const int tid, ET &ptr) {
		ET ret = ptr;
		FLUSH(tid, &ptr);
	#ifndef IZ_NO_FENCE_AFTER_LOAD
		FENCE(tid);
	#endif
		return ret;
	}

	template <class ET>
	inline void store(const int tid, ET &ptr, ET val) {
		ptr = val;
		FLUSH(tid, &ptr);
	}

	template <class ET>
	inline void store(const int tid, ET volatile &ptr, ET val) {
		ptr = val;
		FLUSH(tid, &ptr);
	}

	// link and persist makes the assumption that the same value is never written twice
	// maybe having garbae colelction fits the assumption as well.
	template <class ET>
	inline void store_rel(const int tid, ET &ptr, ET val) {
		FENCE(tid);
		ptr = val;
		FLUSH(tid, &ptr);
	}

	// link and persist makes the assumption that the same value is never written twice
	// maybe having garbae colelction fits the assumption as well.
	template <class ET>
	inline void store_rel(const int tid, ET volatile &ptr, ET val) {
		FENCE(tid);
		ptr = val;
		FLUSH(tid, &ptr);	
	}

	// we assume you do CAS with a value you previously read.
	// This ismplifies things in the link and persist case because
	// that value must have been flushed before you reach this method.
	// That argument also assumes that the same value is not writen twice.
	template <class ET>
	inline bool casb(const int tid, ET volatile *ptr, ET oldv, ET newv) { 
	#if defined(IZ_NO_FENCE_AFTER_LOAD)
		return flush_mark::fence_casb_flush(tid, ptr, oldv, newv);
	#else
		return flush_mark::fence_casb_flush_fence(tid, ptr, oldv, newv);
	#endif	
	}

	// we assume you do CAS with a value you previously read.
	// This ismplifies things in the link and persist case because
	// that value must have been flushed before you reach this method.
	// That argument also assumes that the same value is not writen twice.
	template <class ET>
	inline ET casv(const int tid, ET volatile *ptr, ET oldv, ET newv) { 
	#if defined(IZ_NO_FENCE_AFTER_LOAD)
		return flush_mark::fence_casv_flush(tid, ptr, oldv, newv);
	#else
		return flush_mark::fence_casv_flush_fence(tid, ptr, oldv, newv);
	#endif	
	}

	inline void end_operation(const int tid) {
		FENCE(tid);
	}
}

// I would recommend aliasing the namesapces before using them.
// Only use operations within the namespace. For example don't call FENCE or FLUSH, instead use traversal_datastructure_transformation::flush.
// Notice that izrealivitz does not have flush or fence because you should not have to use it.
// For end_operation(), it doesn't do anything special, just adds a fence, but you should still call it instead of putting your own fance
// because it makes the code more readable and also because we might implement some optimizations in the future where end_operation will become important.
// traversal_datastructure_transformation provides both a flush and flush_node operation. These operations are called between the travesal and the critical section.
// Since you are dealing with the linked list, you can use flush for everything because each node only has one field that needs to be flushed. When there are mutliple mutable fields per node,
// flush_node because more important.
// im_read doesn't do anything special, but it tells the reader that this is a immutable read. It should be inlined so there shouldn't be any function call overhead.
// It's up to you if you use it or not.
// Notice that some operations take pointers (ET*) while others take references (ET&) be careful about this when you use them.

namespace traversal_datastructure_transformation {

	/* tr_read stands for mutalbe reads in the traversal section.
	*/
	template <class ET>
	inline ET tr_read(const int tid, ET &ptr) {
		return ptr;
	}

	/* Use this method for writes that are not part of the critical section
	   and also not part of the initialization section.
	   For initialization, you can perform regular writes without going through this interface.
	*/
	template <class ET>
	inline void write(const int tid, ET &ptr, ET val) {
		ptr = val;
		FLUSH(tid, &ptr);
	}

	/* Use this method for writes that are not part of the critical section
	   and also not part of the initialization section.
	   For initialization, you can perform regular writes without going through this interface.
	*/
	template <class ET>
	inline void write(const int tid, ET volatile &ptr, ET val) {
		ptr = val;
		FLUSH(tid, &ptr);
	}

	/* im_read stands for immutable read in critical or traversal
	   If LINK_AND_PERSIST is defined, then we use the second 
	   least significant bit of a value to do marking.
	*/
	template <class ET>
	inline ET im_read(const int tid, ET &ptr) {
		return ptr;
	}

	/* cr_read stands for critical read
	   NODE_T is only used if FLUSH_MARK_NODE is defined.
	   If LINK_AND_PERSIST is defined, then we use the second 
	   least significant bit of a value to do marking.
	*/
	template <class ET>
	inline ET cr_read(const int tid, ET &ptr) {
		ET ret = ptr;
		FLUSH(tid, &ptr);
		return ret;
	}

	/* cr_write stands for critical section write
	   NODE_T is only used if FLUSH_MARK_NODE is defined.
	   If LINK_AND_PERSIST is defined, then we use the second 
	   least significant bit of a value to do marking.
	*/
	template <class ET>
	inline void cr_write(const int tid, ET &ptr, ET val) {
		FENCE(tid);
		ptr = val;
		FLUSH(tid, &ptr);
	}	

	// we assume you do CAS with a value you previously read.
	// This ismplifies things in the link and persist case because
	// that value must have been flushed before you reach this method.
	// That argument also assumes that the same value is not writen twice.
	template <class ET>
	inline bool cr_casb(const int tid, ET volatile *ptr, ET oldv, ET newv) { 
		return flush_mark::fence_casb_flush(tid, ptr, oldv, newv);
	}

	// we assume you do CAS with a value you previously read.
	// This ismplifies things in the link and persist case because
	// that value must have been flushed before you reach this method.
	// That argument also assumes that the same value is not writen twice.
	template <class ET>
	inline ET cr_casv(const int tid, ET volatile *ptr, ET oldv, ET newv) { 
		return flush_mark::fence_casv_flush(tid, ptr, oldv, newv);
	}

	template <class ET>
	inline void flush(const int tid, ET volatile *ptr) { 
		return FLUSH(tid, ptr);
	}

	// Flush only mutable fields of a node
	// This function should be used for make_persistent and ensure_reachable
	// size represents the number of bytes to flush starting from the location
	// pointed to by n
	template <class NODE_T>
	inline void flush_node(const int tid, NODE_T *n, size_t size) { 
		//cout << "FLUSH_STRUCT(" << (uint64_t) ptr << " " << size << ")" << endl;
		for(uint64_t p = ((uint64_t) n)&CACHELINE_MASK; p < ((uint64_t) n) + size; p += 64ULL)
			//cout << p << endl;
			FLUSH(tid, (void*) p);
	}

	// Flush only mutable fields of a node
	// This function should be used for make_persistent and ensure_reachable
	template <class NODE_T>
	inline void flush_node(const int tid, NODE_T *n) { 
		return flush_node(tid, n, sizeof(NODE_T));
	}

	inline void end_operation(const int tid) {
		FENCE(tid);
	}
}



#endif /* NEW_PMEM_UTILS_H_*/

