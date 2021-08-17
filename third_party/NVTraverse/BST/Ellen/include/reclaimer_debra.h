/**
 * C++ record manager implementation (PODC 2015) by Trevor Brown.
 * 
 * Copyright (C) 2015 Trevor Brown
 *
 */

#ifndef RECLAIM_EPOCH_H
#define	RECLAIM_EPOCH_H

#include <atomic>
#include <cassert>
#include <iostream>
#include <sstream>
#include <limits.h>
#include "blockbag.h"
#include "plaf.h"
#include "allocator_interface.h"
#include "reclaimer_interface.h"

template <typename T = void, class Pool = pool_interface<T> >
class reclaimer_debra : public reclaimer_interface<T, Pool> {
protected:
#define EPOCH_INCREMENT 2
#define BITS_EPOCH(ann) ((ann)&~(EPOCH_INCREMENT-1))
#define QUIESCENT(ann) ((ann)&1)
#define GET_WITH_QUIESCENT(ann) ((ann)|1)

#ifdef RAPID_RECLAMATION
#define MIN_OPS_BEFORE_READ 1
//#define MIN_OPS_BEFORE_CAS_EPOCH 1
#else
#define MIN_OPS_BEFORE_READ 20
//#define MIN_OPS_BEFORE_CAS_EPOCH 100
#endif
    
#define NUMBER_OF_EPOCH_BAGS 9
#define NUMBER_OF_ALWAYS_EMPTY_EPOCH_BAGS 3

    class ThreadData {
    private:
        PAD;
    public:
        std::atomic_long announcedEpoch;
        long localvar_announcedEpoch; // copy of the above, but without the volatile tag, to try to make the read in enterQstate more efficient
    private:
        PAD;
    public:
        blockbag<T> * epochbags[NUMBER_OF_EPOCH_BAGS];
        // note: oldest bag is number (index+1)%NUMBER_OF_EPOCH_BAGS
        int index; // index of currentBag in epochbags for this process
    private:
        PAD;
    public:
        blockbag<T> * currentBag;  // pointer to current epoch bag for this process
        int checked;               // how far we've come in checking the announced epochs of other threads
        int opsSinceRead;
        ThreadData() {}
    private:
        PAD;
    };
    
    PAD;
    ThreadData threadData[MAX_THREADS_POW2];
    PAD;
    
    // for epoch based reclamation
//    PAD; // not needed after superclass layout
    volatile long epoch;
    PAD;
    
public:
    template<typename _Tp1>
    struct rebind {
        typedef reclaimer_debra<_Tp1, Pool> other;
    };
    template<typename _Tp1, typename _Tp2>
    struct rebind2 {
        typedef reclaimer_debra<_Tp1, _Tp2> other;
    };
    
    inline void getSafeBlockbags(const int tid, blockbag<T> ** bags) {
        SOFTWARE_BARRIER;
        int ix = threadData[tid].index;
        bags[0] = threadData[tid].epochbags[ix];
        bags[1] = threadData[tid].epochbags[(ix+NUMBER_OF_EPOCH_BAGS-1)%NUMBER_OF_EPOCH_BAGS];
        bags[2] = threadData[tid].epochbags[(ix+NUMBER_OF_EPOCH_BAGS-2)%NUMBER_OF_EPOCH_BAGS];
        bags[3] = NULL;
        SOFTWARE_BARRIER;
    }
    
    long long getSizeInNodes() {
        long long sum = 0;
        for (int tid=0;tid<this->NUM_PROCESSES;++tid) {
            for (int j=0;j<NUMBER_OF_EPOCH_BAGS;++j) {
                sum += threadData[tid].epochbags[j]->computeSize();
            }
        }
        return sum;
    }
    std::string getSizeString() {
        std::stringstream ss;
        ss<<getSizeInNodes(); //<<" in epoch bags";
        return ss.str();
    }
    
    std::string getDetailsString() {
        std::stringstream ss;
        long long sum[NUMBER_OF_EPOCH_BAGS];
        for (int j=0;j<NUMBER_OF_EPOCH_BAGS;++j) {
            sum[j] = 0;
            for (int tid=0;tid<this->NUM_PROCESSES;++tid) {
                sum[j] += threadData[tid].epochbags[j]->computeSize();
            }
            ss<<sum[j]<<" ";
        }
        return ss.str();
    }
    
    inline static bool quiescenceIsPerRecordType() { return false; }
    
    inline bool isQuiescent(const int tid) {
        return QUIESCENT(threadData[tid].announcedEpoch.load(std::memory_order_relaxed));
    }

    inline static bool isProtected(const int tid, T * const obj) {
        return true;
    }
    inline static bool isQProtected(const int tid, T * const obj) {
        return false;
    }
    inline static bool protect(const int tid, T * const obj, CallbackType notRetiredCallback, CallbackArg callbackArg, bool memoryBarrier = true) {
        return true;
    }
    inline static void unprotect(const int tid, T * const obj) {}
    inline static bool qProtect(const int tid, T * const obj, CallbackType notRetiredCallback, CallbackArg callbackArg, bool memoryBarrier = true) {
        return true;
    }
    inline static void qUnprotectAll(const int tid) {}
    
    inline static bool shouldHelp() { return true; }
    
    // rotate the epoch bags and reclaim any objects retired two epochs ago.
    inline void rotateEpochBags(const int tid) {
        int nextIndex = (threadData[tid].index+1) % NUMBER_OF_EPOCH_BAGS;
        blockbag<T> * const freeable = threadData[tid].epochbags[(nextIndex+NUMBER_OF_ALWAYS_EMPTY_EPOCH_BAGS) % NUMBER_OF_EPOCH_BAGS];
        GSTATS_APPEND(tid, limbo_reclamation_event_size, freeable->computeSize());
        this->pool->addMoveFullBlocks(tid, freeable); // moves any full blocks (may leave a non-full block behind)
        SOFTWARE_BARRIER;
        threadData[tid].index = nextIndex;
        threadData[tid].currentBag = threadData[tid].epochbags[nextIndex];
    }

    template <typename... Rest>
    class BagRotator {
    public:
        BagRotator() {}
        inline void rotateAllEpochBags(const int tid, void * const * const reclaimers, const int i) {
        }
    };

    template <typename First, typename... Rest>
    class BagRotator<First, Rest...> : public BagRotator<Rest...> {
    public:
        inline void rotateAllEpochBags(const int tid, void * const * const reclaimers, const int i) {
            typedef typename Pool::template rebindAlloc<First>::other classAlloc;
            typedef typename Pool::template rebind2<First, classAlloc>::other classPool;

            ((reclaimer_debra<First, classPool> * const) reclaimers[i])->rotateEpochBags(tid);
            ((BagRotator<Rest...> *) this)->rotateAllEpochBags(tid, reclaimers, 1+i);
        }
    };

    // objects reclaimed by this epoch manager.
    // returns true if the call rotated the epoch bags for thread tid
    // (and reclaimed any objects retired two epochs ago).
    // otherwise, the call returns false.
    template <typename First, typename... Rest>
    inline bool startOp(const int tid, void * const * const reclaimers, const int numReclaimers, const bool readOnly = false) {
        SOFTWARE_BARRIER; // prevent any bookkeeping from being moved after this point by the compiler.
        bool result = false;

        long readEpoch = epoch;
        const long ann = threadData[tid].localvar_announcedEpoch;
        threadData[tid].localvar_announcedEpoch = readEpoch;

        // if our announced epoch was different from the current epoch
        if (readEpoch != ann /* invariant: ann is not quiescent */) {
            // rotate the epoch bags and
            // reclaim any objects retired two epochs ago.
            threadData[tid].checked = 0;
            BagRotator<First, Rest...> rotator;
            rotator.rotateAllEpochBags(tid, reclaimers, 0);
            //this->template rotateAllEpochBags<First, Rest...>(tid, reclaimers, 0);
            result = true;
        }
        // we should announce AFTER rotating bags if we're going to do so!!
        // (very problematic interaction with lazy dirty page purging in jemalloc triggered by bag rotation,
        //  which causes massive non-quiescent regions if non-Q announcement happens before bag rotation)
        SOFTWARE_BARRIER;
        threadData[tid].announcedEpoch.store(readEpoch, std::memory_order_relaxed); // note: this must be written, regardless of whether the announced epochs are the same, because the quiescent bit will vary
#if defined USE_GSTATS
        GSTATS_SET(tid, thread_announced_epoch, readEpoch);
#endif
        // note: readEpoch, when written to announcedEpoch[tid],
        //       sets the state to non-quiescent and non-neutralized

#ifndef DEBRA_DISABLE_READONLY_OPT
        if (!readOnly) {
#endif
            // incrementally scan the announced epochs of all threads
            if (++threadData[tid].opsSinceRead == MIN_OPS_BEFORE_READ) {
                threadData[tid].opsSinceRead = 0;
                int otherTid = threadData[tid].checked;
                long otherAnnounce = threadData[otherTid].announcedEpoch.load(std::memory_order_relaxed);
                if (BITS_EPOCH(otherAnnounce) == readEpoch || QUIESCENT(otherAnnounce)) {
                    const int c = ++threadData[tid].checked;
                    if (c >= this->NUM_PROCESSES /*&& c > MIN_OPS_BEFORE_CAS_EPOCH*/) {
                        if (__sync_bool_compare_and_swap(&epoch, readEpoch, readEpoch+EPOCH_INCREMENT)) {
#if defined USE_GSTATS
                            GSTATS_SET_IX(tid, num_prop_epoch_latency, GSTATS_TIMER_SPLIT(tid, timer_epoch_latency), readEpoch+EPOCH_INCREMENT);
#endif
                        }
                    }
                }
            }
#ifndef DEBRA_DISABLE_READONLY_OPT
        }
#endif
        return result;
    }
    
    inline void endOp(const int tid) {
        threadData[tid].announcedEpoch.store(GET_WITH_QUIESCENT(threadData[tid].localvar_announcedEpoch), std::memory_order_relaxed);
    }
    
    // for all schemes except reference counting
    inline void retire(const int tid, T* p) {
        threadData[tid].currentBag->add(p);
        DEBUG2 this->debug->addRetired(tid, 1);
    }
    
    void debugPrintStatus(const int tid) {
        if (tid == 0) {
            std::cout<<"global_epoch_counter="<<epoch<<std::endl;
        }
    }

    void initThread(const int tid) {
        threadData[tid].currentBag = threadData[tid].epochbags[0];
        threadData[tid].opsSinceRead = 0;
        threadData[tid].checked = 0;
        for (int i=0;i<NUMBER_OF_EPOCH_BAGS;++i) {
            threadData[tid].epochbags[i] = new blockbag<T>(tid, this->pool->blockpools[tid]);
        }
    }
    
    reclaimer_debra(const int numProcesses, Pool *_pool, debugInfo * const _debug, RecoveryMgr<void *> * const _recoveryMgr = NULL)
            : reclaimer_interface<T, Pool>(numProcesses, _pool, _debug, _recoveryMgr) {
        VERBOSE std::cout<<"constructor reclaimer_debra helping="<<this->shouldHelp()<<std::endl;// scanThreshold="<<scanThreshold<<std::endl;
        epoch = 0;
        for (int tid=0;tid<numProcesses;++tid) {
            threadData[tid].index = 0;
            threadData[tid].localvar_announcedEpoch = GET_WITH_QUIESCENT(0);
            threadData[tid].announcedEpoch.store(GET_WITH_QUIESCENT(0), std::memory_order_relaxed);
            for (int i=0;i<NUMBER_OF_EPOCH_BAGS;++i) {
                threadData[tid].epochbags[i] = NULL;
            }
        }
    }
    ~reclaimer_debra() {
        VERBOSE DEBUG std::cout<<"destructor reclaimer_debra"<<std::endl;
        for (int tid=0;tid<this->NUM_PROCESSES;++tid) {
            // move contents of all bags into pool
            for (int i=0;i<NUMBER_OF_EPOCH_BAGS;++i) {
                if (threadData[tid].epochbags[i]) {
                    this->pool->addMoveAll(tid, threadData[tid].epochbags[i]);
                    delete threadData[tid].epochbags[i];
                }
            }
        }
    }

};

#endif


