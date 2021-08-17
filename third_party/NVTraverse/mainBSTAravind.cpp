
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <atomic>
#include <cstring>
#include <unistd.h>
#include <fstream>
#include <pthread.h>
#include <assert.h>

#include "rand_r_32.h"
#include "gc/ssmem.h"
#include "barrier.h"

using namespace std;
__thread ssmem_allocator_t *alloc;
__thread ssmem_allocator_t *allocW;
__thread void* nodes[1024];

#include "BST/Aravind/BSTAravindIz.h"
#include "BST/Aravind/BSTAravindOriginal.h"
#include "BST/Aravind/BSTAravindTraverse.h"

static int NUM_THREADS = 1;
static int DURATION = 5;
static int R_RATIO = 90;
static int KEY_RANGE = 1024;
static int ITERATION = 1;
static string ALG_NAME = "DS";

barrier_t barrier_global;
barrier_t init_barrier;

static std::atomic<bool> stop;

//===================================================================
/* Allocate one core per thread */
static inline void set_cpu(int cpu) {
    assert(cpu > -1);
    int n_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu < n_cpus) {
        int cpu_use = cpu;
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(cpu_use, &mask);
        pthread_t thread = pthread_self();
        if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &mask) != 0) {
            fprintf(stderr, "Error setting thread affinity\n");
        }
    }
}

//===================================================================
static void printHelp() {
    cout << "  -A     algorithm" << endl;
    cout << "  -T     thread num" << endl;
    cout << "  -D     duration" << endl;
    cout << "  -R     lookup ratio (0~100)" << endl;
    cout << "  -K     key range" << endl;
    cout << "  -I     iteration number" << endl;
}

//===================================================================
static bool parseArgs(int argc, char **argv) {
    int arg;
    while ((arg = getopt(argc, argv, "A:T:D:R:K:I:H")) != -1) {
        switch (arg) {
            case 'A':
                ALG_NAME = string(optarg);
                break;
            case 'T':
                NUM_THREADS = atoi(optarg);
                break;
            case 'D':
                DURATION = atoi(optarg);
                break;
            case 'R':
                R_RATIO = atoi(optarg);
                break;
            case 'K':
                KEY_RANGE = atoi(optarg);
                break;
            case 'I':
                ITERATION = atoi(optarg);
                break;
            case 'H':
                printHelp();
                return false;
            default:
                return false;
        }
    }
    return true;
}

//===================================================================
struct thread_data {
    uint32_t tid;
    void* set;
    uint64_t ops;
};

//===================================================================
template <class SET>
void threadRun(thread_data *tData, bool nv) {
    uint32_t rRatio = R_RATIO * 10;
    uint32_t iRatio = rRatio + (1000 - rRatio) / 2;
    uint32_t id = tData->tid;
    set_cpu(id - 1);
    uint32_t seed1 = id;
    uint32_t seed2 = seed1 + 1;

    alloc = (ssmem_allocator_t *)malloc(sizeof(ssmem_allocator_t));
    ssmem_alloc_init(alloc, SSMEM_DEFAULT_MEM_SIZE, id, nv);

    allocW = (ssmem_allocator_t *)malloc(sizeof(ssmem_allocator_t));
    ssmem_alloc_init(allocW, SSMEM_DEFAULT_MEM_SIZE, id, nv);

    barrier_cross(&init_barrier);

    uint32_t elementsToInsert = (uint32_t)((KEY_RANGE / 2) / NUM_THREADS);
    uint32_t missingElements = (uint32_t)(KEY_RANGE / 2) - (elementsToInsert * NUM_THREADS);
    if (id <= missingElements) {
        elementsToInsert++;
    }

    uint64_t ops = 0;
    SET* set = (SET *)tData->set;
    set->bst_init_local();
    for (int i = 0; i < (uint32_t)elementsToInsert; i++) {
        int key = rand_r_32(&seed2) % KEY_RANGE;
        if (!set->insert(id, key, id)) {
            i--;
        }
    }

    barrier_cross(&barrier_global);
    while (!stop) {
        int op = rand_r_32(&seed1) % 1000;
        int key = rand_r_32(&seed2) % KEY_RANGE;
        if (op < rRatio) {
            set->contains(id, key);
        }
        else if (op < iRatio) {
            set->insert(id, key, id);

        }
        else {
            set->remove(id, key);
        }
        ops++;
    }
    tData->ops = ops;
}

//===================================================================
template <class SET>
static void run(bool nonVolatile) {

    SET* set = new SET();
    if (ITERATION == 1) {
        cout << "##" << ALG_NAME << " - Initial: " << KEY_RANGE / 2 << " / Range: " << KEY_RANGE;
        cout << " / Update: " << 100 - R_RATIO << " / Threads: " << NUM_THREADS << endl;
    }

    barrier_init(&barrier_global, NUM_THREADS + 1);
    barrier_init(&init_barrier, NUM_THREADS);

    alloc = (ssmem_allocator_t *)malloc(sizeof(ssmem_allocator_t));
    ssmem_alloc_init(alloc, SSMEM_DEFAULT_MEM_SIZE, 0, nonVolatile);

    stop = (false);

    thread* threads[NUM_THREADS];
    thread_data allThreadsData[NUM_THREADS];


    for (uint32_t j = 1; j < NUM_THREADS + 1; j++) {
        thread_data &tData = allThreadsData[j - 1];
        tData.tid = j;
        tData.set = set;
        tData.ops = 0;
        threads[j - 1] = new thread(threadRun<SET>, &tData, nonVolatile);
    }

    barrier_cross(&barrier_global);

    sleep(DURATION);

    stop = (true);
    for (uint32_t j = 0; j < NUM_THREADS; j++)
        threads[j]->join();

    uint64_t totalOps = 0;
    for (uint32_t j = 0; j < NUM_THREADS; j++) {
        totalOps += allThreadsData[j].ops;
    }

    cout << totalOps / (DURATION) << endl;
}

//===================================================================
int main(int argc, char **argv) {
    if (!parseArgs(argc, argv)) {
        return 0;
    }

    if (!ALG_NAME.compare("BSTAravindOriginal")) {
            run<BSTAravindOriginal<int>>(false);
    } else if (!ALG_NAME.compare("BSTAravindIz")) {
            run<BSTAravindIz<int>>(true);
    } else if (!ALG_NAME.compare("BSTAravindTraverse")) {
            run<BSTAravindTraverse<int>>(true);
    }
    else {
        cout << "Algorithm does not exist!" << endl;
        cout << ALG_NAME << endl;
    }
    return 0;
}

