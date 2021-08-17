from engines.replayengine import CacheBase, PermutatorBase, ReplayEngineBase
from mem.memoryoperations import TXStart, TXEnd, PMDKCall, Store, Flush, Fence
from misc.witcherexceptions import NotSupportedOperationException
from logging import getLogger
from itertools import permutations
import csv

# This is the cache used by pmreorder engine,
# it doesn't care about the cacheline
class PMReorderCache(CacheBase):
    def __init__(self):
        self.stores_list = []

    def accept(self, op):
        def accept_store(store_op):
            self.stores_list.append(store_op)

        def accept_flush(flush_op):
            for store_op in self.stores_list:
                if flush_op.is_in_flush(store_op):
                    store_op.flushed = 1

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
        # TODO flush those writes to the memory
        self.stores_list = \
            list( filter(lambda x: x.flushed == 0, self.stores_list))

    def get_stores(self):
        return self.stores_list

    def get_stores_to_flush(self):
        return list(filter(lambda x: x.flushed == 1, self.stores_list))

class PMReorderPermutator(PermutatorBase):
    def __init__(self, cache):
        self.cache = cache

    def run(self):
        def pmreorder_permutation_count(length):
            total = 0
            for i in  range(0, length + 1):
                count = 1
                j = 0
                while j < i:
                    count *= length - j
                    j += 1
                total += count
            return total

        permutation_count = 0
        # Only permute flushed stores here!!!!
        stores_list = self.cache.get_stores_to_flush()
        permutation_count += pmreorder_permutation_count(len(stores_list))
        getLogger().debug("permutation_count = " + str(permutation_count))
        return permutation_count

class PMReorderEngine(ReplayEngineBase):
    # initialization
    def __init__(self):
        self.cache = PMReorderCache()
        self.permutator = PMReorderPermutator(self.cache)
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
        with open('pmreorder.csv', 'w', newline='') as file:
            writer = csv.writer(file)
            writer.writerow(['op', 'per_op_count', "total_count"])
            op_count = 0
            total = 0
            for permutation_count in self.permutation_count_list:
                total += permutation_count
                writer.writerow([op_count, permutation_count, total])
                op_count += 1
