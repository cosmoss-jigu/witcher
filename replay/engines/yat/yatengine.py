from engines.replayengine import CachelineBase, CacheBase, PermutatorBase, ReplayEngineBase
from mem.memoryoperations import TXStart, PMDKCall, TXEnd, Store, Flush, Fence
from misc.utils import Rangeable, range_cmp
from mem.cachenumbers import CACHELINE_BYTES, ATOMIC_WRITE_BYTES, get_cacheline_address
from logging import getLogger
import csv

# A YatCachelineBucket is an address range
# It maintains a list of stores writing to this address range
class YatCachelineBucket(Rangeable):
    def __init__(self, address, size):
        self.address = address
        self.size = size
        self.stores_list = []

    def accept_store(self, store_op):
        self.stores_list.append(store_op)

    def accept_flush(self):
        for store_op in self.stores_list:
            store_op.flushed = 1

    def flush(self):
        #TODO need to write to the memory
        self.stores_list = \
            list( filter(lambda x: x.flushed == 0, self.stores_list))

    def flush_all(self):
        #TODO need to write to the memory
        self.stores_list = []

    def is_empty(self):
        return len(self.stores_list) == 0

    def get_num_of_stores(self):
        return len(self.stores_list)

    def get_stores(self):
        return self.stores_list

    def get_base_address(self):
        return self.address

    def get_max_address(self):
        return self.address + self.size

# A YatCacheline is a list of buckets
class YatCacheline(CachelineBase):
    def __init__(self, address, size, size_per_bucket):
        self.address = address
        self.size = size
        self.size_per_bucket = size_per_bucket
        self.num_of_buckets = int(size / size_per_bucket)
        self.buckets = []
        for index in range(0, self.num_of_buckets):
            bucket_address = self.address + index * size_per_bucket
            cacheline_bucket = YatCachelineBucket(bucket_address, size_per_bucket)
            self.buckets.append(cacheline_bucket)

    def get_bucket(self, store_op):
        for bucket in self.buckets:
            if range_cmp(bucket, store_op) == 0:
                assert(bucket.get_base_address() <= store_op.get_base_address())
                assert(bucket.get_max_address() >= store_op.get_max_address())
                return bucket
        return None

    def can_accept_store(self, store_op):
        if self.get_bucket(store_op) == None:
            return False
        else:
            return True

    def can_accept_flush(self, flush_op):
        if self.address == flush_op.get_base_address() and \
           self.address + self.size == flush_op.get_max_address():
            return True
        else:
            return False

    def accept_store(self, store_op):
        bucket = self.get_bucket(store_op)
        assert(bucket != None)
        bucket.accept_store(store_op)

    def accept_flush(self):
        for bucket in self.buckets:
            bucket.accept_flush()

    def flush(self):
        for bucket in self.buckets:
            bucket.flush()

    def flush_all(self):
        for bucket in self.buckets:
            bucket.flush_all()

    def is_empty(self):
        for bucket in self.buckets:
            if bucket.is_empty() == False:
                return False
        return True

    def get_num_of_stores(self):
        num_of_stores = 0
        for bucket in self.buckets:
            num_of_stores += bucket.get_num_of_stores()
        return num_of_stores

    def get_stores(self):
        stores = []
        for bucket in self.buckets:
            stores.append(bucket.get_stores())
        return stores

# YatCache is a list of cacheline
class YatCache(CacheBase):
    def __init__(self):
        self.cachelines = []

    def accept(self, op):
        def accept_store(store_op):
            cacheline = None
            for cacheline_it in self.cachelines:
                if (cacheline_it.can_accept_store(store_op)):
                    cacheline = cacheline_it
                    break
            if cacheline == None:
                address = get_cacheline_address(store_op.get_base_address())
                cacheline = YatCacheline(address,
                                      CACHELINE_BYTES,
                                      ATOMIC_WRITE_BYTES)
                self.cachelines.append(cacheline)
            cacheline.accept_store(store_op)

        def accept_flush(flush_op):
            for cacheline in self.cachelines:
                if (cacheline.can_accept_flush(flush_op)):
                    cacheline.accept_flush()
                    break

        time_to_replay_and_flush = False
        if isinstance(op, TXStart):
            pass
        elif isinstance(op, TXEnd):
            pass
        elif isinstance(op, PMDKCall):
            pass
        elif isinstance(op, Store):
            accept_store(op)
        elif isinstance(op, Flush):
            accept_flush(op)
        elif isinstance(op, Fence):
            time_to_replay_and_flush = True
        else:
            raise NotSupportedOperationException(op)
        return time_to_replay_and_flush

    def flush(self):
        empty_cachelines = []
        for cacheline in self.cachelines:
            cacheline.flush()
            if cacheline.is_empty():
                empty_cachelines.append(cacheline)
        for cacheline in empty_cachelines:
            self.cachelines.remove(cacheline)


    def flush_all(self):
        for cacheline in self.cachelines:
            cacheline.flush_all()
        self.cachelines = []

    def get_cachelines(self):
        return self.cachelines

    def get_stores(self):
        stores = []
        for cacheline in self.cachelines:
            stores.append(cacheline.get_stores())
        return stores

class YatPermutator(PermutatorBase):
    def __init__(self, cache):
        self.cache = cache

    # (size of CL + 1) * ... * (size of CL + 1) - 1
    def run(self):
        # Permute all stores in the cache
        permutation_count = 1
        for cacheline in self.cache.get_cachelines():
            num_of_stores = cacheline.get_num_of_stores()
            permutation_count *= num_of_stores + 1
        permutation_count -= 1
        return permutation_count

class YatEngine(ReplayEngineBase):
    # initialization
    def __init__(self):
        self.cache = YatCache()
        self.permutator = YatPermutator(self.cache)
        self.permutation_count_list = []
        self.permutation_count = 0

    # set the trace
    def set_trace(self, trace):
        self.trace = trace

    # traverse all the operations in the trace
    def run(self):
        # start from the first tx
        ops = self.trace.atomic_write_ops
        first_tx_start_index = self.trace.atomic_write_ops_tx_ranges[0][0]
        ops = ops[first_tx_start_index:]
        for op in ops:
            if isinstance(op, TXStart):
                self.permutation_count = 0
            if isinstance(op, TXEnd):
                self.permutation_count_list.append(self.permutation_count)
            if self.cache.accept(op) == True:
                getLogger().debug("Before Fence: " +
                                  str(self.cache.get_stores()))
                self.permutation_count += self.permutator.run()
                self.cache.flush()
                getLogger().debug("After Fence: " +
                                  str(self.cache.get_stores()))
        getLogger().debug("In the end: " + str(self.cache.get_stores()))
        getLogger().debug("permutation_count_list = " +
                          str(self.permutation_count_list))

        self.epilogue()

    # write the result into a file
    def epilogue(self):
        with open('yat.csv', 'w', newline='') as file:
            writer = csv.writer(file)
            writer.writerow(['op', 'per_op_count', "total_count"])
            op_count = 0
            total = 0
            for permutation_count in self.permutation_count_list:
                total += permutation_count
                writer.writerow([op_count, permutation_count, total])
                op_count += 1
