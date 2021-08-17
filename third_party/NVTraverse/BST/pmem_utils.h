#ifndef PMEM_UTILS_H_
#define PMEM_UTILS_H_

#include <iostream>
#define PWB_IS_CLFLUSH 1

template <class ET>
inline bool CASB(ET volatile *ptr, ET oldv, ET newv) { 
	bool ret;
	if (sizeof(ET) == 1) { 
		ret = __sync_bool_compare_and_swap_1((bool*) ptr, *((bool*) &oldv), *((bool*) &newv));
	} else if (sizeof(ET) == 8) {
		ret = __sync_bool_compare_and_swap_8((long*) ptr, *((long*) &oldv), *((long*) &newv));
	} else if (sizeof(ET) == 4) {
		ret = __sync_bool_compare_and_swap_4((int *) ptr, *((int *) &oldv), *((int *) &newv));
	} 
#if defined(MCX16)
	else if (sizeof(ET) == 16) {
		ret = __sync_bool_compare_and_swap_16((__int128*) ptr,*((__int128*)&oldv),*((__int128*)&newv));
	}
#endif
	else {
		std::cout << "CAS bad length (" << sizeof(ET) << ")" << std::endl;
		abort();
	}
	return ret;
}

template <class ET>
inline ET CASV(ET volatile *ptr, ET oldv, ET newv) { 
	ET ret;
	if (sizeof(ET) == 1) { 
		std::cout << "CASV bad length (" << sizeof(ET) << ")" << std::endl;
		abort();
	} else if (sizeof(ET) == 8) {
		ret = (ET) __sync_val_compare_and_swap_8((long*) ptr, *((long*) &oldv), *((long*) &newv));
//return utils::LCAS((long*) ptr, *((long*) &oldv), *((long*) &newv));
	} else if (sizeof(ET) == 4) {
		std::cout << "CASV bad length (" << sizeof(ET) << ")" << std::endl;
		abort();
//return utils::SCAS((int *) ptr, *((int *) &oldv), *((int *) &newv));
	} 
#if defined(MCX16)
	else if (sizeof(ET) == 16) {
		ret = (ET) __sync_val_compare_and_swap_16((__int128*) ptr,*((__int128*)&oldv),*((__int128*)&newv));
	}
#endif
	else {
		std::cout << "CASV bad length (" << sizeof(ET) << ")" << std::endl;
		abort();
	}
	return ret;
}



#define MFENCE __sync_synchronize
#define SAME_CACHELINE(a, b) ((((uint64_t)(a))>>6) == (((uint64_t)(b))>>6))
const uint64_t CACHELINE_MASK = ~(64ULL-1ULL);

#define DUMMY_TID -1
#ifdef PMEM_STATS
	#define PMEM_STATS_PADDING 128
	#define MAX_THREADS_POW2 256
	static long long flush_count[MAX_THREADS_POW2*PMEM_STATS_PADDING]; //initialized to 0
	static long long fence_count[MAX_THREADS_POW2*PMEM_STATS_PADDING];

	void print_pmem_stats() {
		long long flush_count_agg = 0;
		long long fence_count_agg = 0;
		for(int i = 0; i < MAX_THREADS_POW2; i++) {
			flush_count_agg += flush_count[i*PMEM_STATS_PADDING];
			fence_count_agg += fence_count[i*PMEM_STATS_PADDING];
		}
		std::cout << "Flush count: " << flush_count_agg << std::endl;
		std::cout << "Fence count: " << fence_count_agg << std::endl;
	}
#endif

template <class ET>
	inline void FLUSH(const int tid, ET *p)
	{
	#ifdef PMEM_STATS
		flush_count[tid*PMEM_STATS_PADDING]++;
	#endif

	#ifdef PWB_IS_CLFLUSH
		//asm volatile ("clflush (%0)" :: "r"(p));
    asm volatile("clflush %0" : "+m" (*(volatile char *)p));
	#elif PWB_IS_CLFLUSHOPT
	    asm volatile(".byte 0x66; clflush %0" : "+m" (*(volatile char *)(p)));    // clflushopt (Kaby Lake)
	#elif PWB_IS_CLWB
	    asm volatile(".byte 0x66; xsaveopt %0" : "+m" (*(volatile char *)(p)));  // clwb() only for Ice Lake onwards
	#else
	#error "You must define what PWB is. Choose PWB_IS_CLFLUSH if you don't know what your CPU is capable of"
	#endif
	}

// assumes that ptr + size will not go out of the struct
// also assumes that structs fit in one cache line when aligned
template <class ET>
	inline void FLUSH_STRUCT(const int tid, ET *ptr, size_t size)
	{
	#if defined(CACHE_ALIGN)
		FLUSH(tid, ptr);
	#else
		//cout << "FLUSH_STRUCT(" << (uint64_t) ptr << " " << size << ")" << endl;
		for(uint64_t p = ((uint64_t) ptr)&CACHELINE_MASK; p < ((uint64_t) ptr) + size; p += 64ULL)
			//cout << p << endl;
			FLUSH(tid, (void*) p);
	#endif
	}	

template <class ET>
	inline void FLUSH_STRUCT(const int tid, ET *ptr)
	{
	#if defined(CACHE_ALIGN)
		FLUSH(tid, ptr);
	#else
		FLUSH_STRUCT(tid, ptr, sizeof(ET));
	#endif
	//for(char *p = (char *) ptr; (uint64_t) p < (uint64_t) (ptr+1); p += 64)
	//	FLUSH(p);
	}	

// flush word pointed to by ptr in node n
template <class ET, class NODE_T>
	inline void FLUSH_node(const int tid, ET *ptr, NODE_T *n)
	{
	//if(!SAME_CACHELINE(ptr, n))
	//	std::cerr << "FLUSH NOT ON SAME_CACHELINE" << std::endl;
	#ifdef MARK_FLUSHED
		if(n->flushed)
			FLUSH(tid, ptr);
	#else
		FLUSH(tid, ptr);
	#endif
	}

// flush entire node pointed to by ptr
template <class ET>
	inline void FLUSH_node(const int tid, ET *ptr)
	{
	#ifdef MARK_FLUSHED
		if(ptr->flushed)
			FLUSH_STRUCT(tid, ptr);
	#else
		FLUSH_STRUCT(tid, ptr);
	#endif
	}


	inline void SFENCE()
	{
		//asm volatile ("sfence" ::: "memory");
    asm volatile("mfence":::"memory");
	}

	inline void FENCE(const int tid)
	{
	#ifdef PMEM_STATS
		fence_count[tid*PMEM_STATS_PADDING]++;
	#endif

	#ifdef PWB_IS_CLFLUSH
		//MFENCE();
    asm volatile("mfence":::"memory");
	#elif PWB_IS_CLFLUSHOPT
		SFENCE();
	#elif PWB_IS_CLWB
		SFENCE();
	#else
	#error "You must define what PWB is. Choose PWB_IS_CLFLUSH if you don't know what your CPU is capable of"
	#endif
	}

#define BARRIER(tid, p) {FLUSH(tid, p);FENCE(tid);}

#ifdef IZ
	#define IF_IZ if(1)
	#define IF_OURS if(0)
#else
	#define IF_OURS if(1)
	#define IF_IZ if(0)
#endif

	namespace pmem_utils {
	// The conditional should be removed by the compiler
	// this should work with pointer types, or pairs of integers

	template <class ET>
		inline bool FCAS(const int tid, ET volatile *ptr, ET oldv, ET newv) { 
			bool ret;
			if (sizeof(ET) == 1) { 
				ret = __sync_bool_compare_and_swap_1((bool*) ptr, *((bool*) &oldv), *((bool*) &newv));
			} else if (sizeof(ET) == 8) {
				ret = __sync_bool_compare_and_swap_8((long*) ptr, *((long*) &oldv), *((long*) &newv));
			} else if (sizeof(ET) == 4) {
				ret = __sync_bool_compare_and_swap_4((int *) ptr, *((int *) &oldv), *((int *) &newv));
			} 
#if defined(MCX16)
			else if (sizeof(ET) == 16) {
				ret = __sync_bool_compare_and_swap_16((__int128*) ptr,*((__int128*)&oldv),*((__int128*)&newv));
			}
#endif
			else {
				std::cout << "CAS bad length (" << sizeof(ET) << ")" << std::endl;
				abort();
			}
			FLUSH(tid, ptr);
#if defined(IZ) && !defined(MARK_FLUSHED)
			FENCE(tid);
#endif
			return ret;
		}

	template <class ET>
		inline ET FCASV(const int tid, ET volatile *ptr, ET oldv, ET newv) { 
			ET ret;
			if (sizeof(ET) == 1) { 
				std::cout << "CASV bad length (" << sizeof(ET) << ")" << std::endl;
				abort();
			} else if (sizeof(ET) == 8) {
				ret = (ET) __sync_val_compare_and_swap_8((long*) ptr, *((long*) &oldv), *((long*) &newv));
		//return utils::LCAS((long*) ptr, *((long*) &oldv), *((long*) &newv));
			} else if (sizeof(ET) == 4) {
				std::cout << "CASV bad length (" << sizeof(ET) << ")" << std::endl;
				abort();
		//return utils::SCAS((int *) ptr, *((int *) &oldv), *((int *) &newv));
			} 
	#if defined(MCX16)
			else if (sizeof(ET) == 16) {
				ret = (ET) __sync_val_compare_and_swap_16((__int128*) ptr,*((__int128*)&oldv),*((__int128*)&newv));
			}
	#endif
			else {
				std::cout << "CASV bad length (" << sizeof(ET) << ")" << std::endl;
				abort();
			}
			FLUSH(tid, ptr);
#if defined(IZ) && !defined(MARK_FLUSHED)
			FENCE(tid);
#endif
			return ret;
		}

	template <class ET>
		inline ET READ(const int tid, ET &ptr)
		{
			ET ret = ptr;
			FLUSH(tid, &ptr);
		#ifdef IZ
			FENCE(tid);
		#endif
			return ret;
		}

	template <class ET>
		inline void WRITE(const int tid, ET volatile &ptr, ET val)
		{
			FENCE(tid);
			ptr = val;
			FLUSH(tid, &ptr);
		}
/*
	// NO_FENCE operations are used at the beginning of every operation
	// Need to also remember to FENCE before returning.
	template <class ET>
		inline ET READ_NO_FENCE(const int tid, ET &ptr)
		{
			ET ret = ptr;
			FLUSH(tid, &ptr);
			return ret;
		}
*/

	template <class ET>
		inline void WRITE_NO_FENCE(const int tid, ET volatile &ptr, ET val)
		{
			ptr = val;
			FLUSH(tid, &ptr);
		}

	template <class ET, class NODE_T>
		inline ET READ_node(const int tid, ET &ptr, NODE_T *n)
		{
			ET ret = ptr;
			FLUSH_node(tid, &ptr, n);
		#ifdef IZ
			FENCE(tid);
		#endif
			return ret;
		}

/*
	template <class ET, class NODE_T>
	inline ET READ_NO_FENCE_node(const int tid, ET &ptr, NODE_T *n)
	{
		ET ret = ptr;
		FLUSH_node(tid, &ptr, n);
		return ret;
	}
*/
	template <class ET, class NODE_T>
		inline void WRITE_node(const int tid, ET volatile &ptr, ET val, NODE_T *n)
		{
		//if(!SAME_CACHELINE(&ptr, n))
		//	std::cerr << "WRITE NOT ON SAME_CACHELINE" << std::endl;
		#ifdef MARK_FLUSHED
			__sync_fetch_and_add(&n->flushed, 1);
		#endif
			WRITE(tid, ptr, val);
		#ifdef MARK_FLUSHED
			__sync_fetch_and_add(&n->flushed, -1);
		#endif
		}

	template <class ET, class NODE_T>
		inline bool FCAS_node(const int tid, ET volatile *ptr, ET oldv, ET newv, NODE_T *n) { 
		#ifdef MARK_FLUSHED
			__sync_fetch_and_add(&n->flushed, 1);
		#endif
			bool ret = FCAS(tid, ptr, oldv, newv);
		#ifdef MARK_FLUSHED
			__sync_fetch_and_add(&n->flushed, -1);
		#endif
			return ret;
		}

	template <class ET, class NODE_T>
		inline ET FCASV_node(const int tid, ET volatile *ptr, ET oldv, ET newv, NODE_T *n) { 
		//if(!SAME_CACHELINE(ptr, n))
		//	std::cerr << "FCASV NOT ON SAME_CACHELINE" << std::endl;
		#ifdef MARK_FLUSHED
			__sync_fetch_and_add(&n->flushed, 1);
		#endif
			ET ret = FCASV(tid, ptr, oldv, newv);
		#ifdef MARK_FLUSHED
			__sync_fetch_and_add(&n->flushed, -1);
		#endif
			return ret;
		}

	/*
	typedef struct
    {
      unsigned __int128 value;
    } __attribute__ ((aligned (16))) atomic_uint128;*/

/*
    unsigned __int128 atomic_read_uint128 (unsigned __int128 *src)
    {
      if((unsigned long long) src & 15ull) cerr << src << " is not 16 byte aligned" << endl;
      unsigned __int128 result;
      asm volatile ("xor %%rax, %%rax;"
                    "xor %%rbx, %%rbx;"
                    "xor %%rcx, %%rcx;"
                    "xor %%rdx, %%rdx;"
                    "lock cmpxchg16b %1" : "=A"(result) : "m"(*src) : "rbx", "rcx");
      return result;
    }*/
	}

#endif /* PMEM_UTILS_H_ */

