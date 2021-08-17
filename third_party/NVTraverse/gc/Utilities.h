/*
 * Utilities.h
 *
 *  Created on: Jul 14, 2016
 *      Author: michal
 */

#ifndef UTILITIES_H_
#define UTILITIES_H_

#include <stddef.h> 			//for null
#include <climits>				//for max int
#include <fstream>

#define MAX_THREADS 144
#define FACTOR 100000
#define PADDING 512             // Padding must be multiple of 4 for proper alignment
#define QUEUE_SIZE 1000000
#define CAS __sync_bool_compare_and_swap
#define MFENCE __sync_synchronize

//std::ofstream file;

inline void FLUSH(void *p)
{
#ifdef PWB_IS_CLFLUSH
  asm volatile ("clflush (%0)" :: "r"(p));
#elif PWB_IS_CLFLUSHOPT
  asm volatile(".byte 0x66; clflush %0" : "+m" (*(volatile char *)(p)));    // clflushopt (Kaby Lake)
#elif PWB_IS_CLWB
  asm volatile(".byte 0x66; xsaveopt %0" : "+m" (*(volatile char *)(p)));  // clwb() only for Ice Lake onwards
#else
#error "You must define what PWB is. Choose PWB_IS_CLFLUSH if you don't know what your CPU is capable of"
#endif
}

inline void FLUSH(volatile void *p)
{
#ifdef PWB_IS_CLFLUSH
  asm volatile ("clflush (%0)" :: "r"(p));
#elif PWB_IS_CLFLUSHOPT
  asm volatile(".byte 0x66; clflush %0" : "+m" (*(volatile char *)(p)));    // clflushopt (Kaby Lake)
#elif PWB_IS_CLWB
  asm volatile(".byte 0x66; xsaveopt %0" : "+m" (*(volatile char *)(p)));  // clwb() only for Ice Lake onwards
#else
#error "You must define what PWB is. Choose PWB_IS_CLFLUSH if you don't know what your CPU is capable of"
#endif
}

inline void SFENCE()
{
    asm volatile ("sfence" ::: "memory");
}

inline void FENCE()
{
#ifdef PMEM_STATS
  fence_count[tid*PMEM_STATS_PADDING]++;
#endif

#ifdef PWB_IS_CLFLUSH
  //MFENCE();
#elif PWB_IS_CLFLUSHOPT
  SFENCE();
#elif PWB_IS_CLWB
  SFENCE();
#else
#error "You must define what PWB is. Choose PWB_IS_CLFLUSH if you don't know what your CPU is capable of"
#endif
}

/*
void BARRIER(void* p){
	MFENCE();
	FLUSH(p);
	MFENCE();
}

void BARRIER(volatile void* p){
	MFENCE();
	FLUSH(p);
	MFENCE();
}



void OPT_BARRIER(void* p){
	MFENCE();
	FLUSH(p);
}

void OPT_BARRIER(volatile void* p){
	MFENCE();
	FLUSH(p);
}*/
/*
#define BARRIER(p) {FLUSH(p);MFENCE();}
#define OPT_BARRIER(p) {FLUSH(p);}

#ifdef MANUAL_FLUSH
	#define MANUAL(x) x
#else
	#define MANUAL(x) {}
#endif

#ifdef READ_WRITE_FLUSH
	#define RFLUSH(x) x
	#define WFLUSH(x) x
#else
	#ifdef WRITE_FLUSH
		#define RFLUSH(x) {}
		#define WFLUSH(x) x
	#else
		#define RFLUSH(x) {}
		#define WFLUSH(x) {}
	#endif
#endif
*/
//Uncomment below for shared model. Comment definitions above.

static void BARRIER(void* p){
	FLUSH(p);
	MFENCE();
}

static void BARRIER(volatile void* p){
	FLUSH(p);
	MFENCE();
}



static void OPT_BARRIER(void* p){
	FLUSH(p);
	//SFENCE();
}

static void OPT_BARRIER(volatile void* p){
	FLUSH(p);
	//SFENCE();
}

static bool BARRIERM(void* p){
	long pLong = (long)p;
	if((pLong & 2) && !(pLong & 1)){
		return false;
	} else {
		FLUSH(p);
		SFENCE();
		return true;
	}
}

static bool BARRIERM(volatile void* p){
	long pLong = (long)p;
	if((pLong & 2) && !(pLong & 1)){
		return false;
	} else {
		FLUSH(p);
		SFENCE();
		return true;
	}
}



static bool OPT_BARRIERM(void* p){
	long pLong = (long)p;
	if((pLong  & 2) && !(pLong & 1)){
		return false;
	} else {
		FLUSH(p);
		SFENCE();
		return true;
	}
}

static bool OPT_BARRIERM(volatile void* p){
	long pLong = (long)p;
	if((pLong & 2) && !(pLong & 1)){
		return false;
	} else {
		FLUSH(p);
		SFENCE();
		return true;
	}
}
static bool FLUSHM(void* p){
        long pLong = (long)p;
        if((pLong & 2) && !(pLong & 1)){
                return false;
        } else {
                FLUSH(p);
                return true;
        }
}

static bool FLUSHM(volatile void* p){
        long pLong = (long)p;
        if((pLong & 2) && !(pLong & 1)){
                return false;
        } else {
                FLUSH(p);
                return true;
        }
}




#endif /* UTILITIES_H_ */

