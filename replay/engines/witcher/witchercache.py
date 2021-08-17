from engines.replayengine import CachelineBase, CacheBase
from mem.memoryoperations import TXStart, TXEnd, Store, Flush, Fence, PMDKCall, PMDKTXAdd, PMDKTXAlloc
from misc.utils import Rangeable, range_cmp
from mem.cachenumbers import CACHELINE_BYTES, ATOMIC_WRITE_BYTES, get_cacheline_address
from misc.witcherexceptions import NotSupportedOperationException
from logging import getLogger

# A WitcherCacheline is a list of buckets
class WitcherCacheline(CachelineBase):
    def __init__(self, address, size, binary_file):
        self.address = address
        self.size = size
        self.binary_file = binary_file
        # a cacheline needs the store list to track store order
        self.stores_list = []

    def accept_store(self, store_op):
        assert(self.can_accept_store_op(store_op))
        # Update the store_list
        self.stores_list.append(store_op)

    def accept_flush(self, flush_op):
        assert(self.can_accept_flush_op(flush_op))
        # Mark all stores as flushing
        for store_op in self.stores_list:
            store_op.accept_flush()

    def write_back_all_flushing_stores(self):
        # Write all flushing stores in the cache to the file
        # and mark them as fenced
        # then update the store list
        for store_op in self.stores_list:
            if store_op.is_flushing():
                self.binary_file.do_store(store_op)
                store_op.accept_fence()
        # Update the store_list
        self.stores_list = \
            list( filter(lambda store: store.is_clear(), self.stores_list))

    def write_back_all_stores(self):
        # Write all stores in the cache to the file
        for store_op in self.stores_list:
            self.binary_file.do_store(store_op)
            store_op.accept_fence()
        # Clear the store_list
        self.stores_list = []

    # get all stores from crash_plan
    # (1) crash_plan itself
    # (2) all stores in the same cacheline happen before it
    def get_stores_from_crash_plan(self, crash_plan):
        assert(self.can_accept_store_op(crash_plan))
        crash_plan_index = self.stores_list.index(crash_plan)
        return self.stores_list[:crash_plan_index+1]

    def is_empty(self):
        return len(self.stores_list) == 0

    def can_accept_store_op(self, store_op):
        return self.address <= store_op.address and \
               store_op.address + store_op.size <= self.address + self.size

    def can_accept_flush_op(self, flush_op):
        return self.address == flush_op.address and \
               self.size == flush_op.size

# WitcherCache is a list of cacheline
class WitcherCache(CacheBase):
    def __init__(self, binary_file):
        # a dict of cachelines: key:addr, val:cacheline
        self.cacheline_dict = dict()
        # binary file for writing
        self.binary_file = binary_file

    # return true when it is a fence
    # it could be the time to crash
    # need flush outside when it returns true
    def accept(self, op):
        getLogger().debug("accepting op: " + str(op))
        is_fence = False
        if isinstance(op, Store):
            self.accept_store(op)
        elif isinstance(op, Flush):
            self.accept_flush(op)
        elif isinstance(op, Fence):
            is_fence = True
        elif isinstance(op, PMDKCall):
            pass
        elif isinstance(op, PMDKTXAdd):
            pass
        elif isinstance(op, PMDKTXAlloc):
            pass
        elif isinstance(op, TXStart) or isinstance(op, TXEnd):
            pass
        else:
            raise NotSupportedOperationException(op)
        return is_fence

    # accept_store
    # create or find the cacheline and let it accept the store
    def accept_store(self, store_op):
        cacheline = self.get_cacheline_from_address(store_op.address)
        cacheline.accept_store(store_op)

    # accept_flush
    # create or find the cacheline and let it accept the flush
    def accept_flush(self, flush_op):
        cacheline = self.get_cacheline_from_address(flush_op.address)
        cacheline.accept_flush(flush_op)

    def write_back_all_flushing_stores(self):
        empty_cachelines = []
        for cacheline in self.get_cachelines():
            cacheline.write_back_all_flushing_stores()
            if cacheline.is_empty():
                empty_cachelines.append(cacheline)
        for cacheline in empty_cachelines:
            del self.cacheline_dict[cacheline.address]

    def write_back_all_stores(self):
        for cacheline in self.get_cachelines():
            cacheline.write_back_all_stores()
        self.cacheline_dict = dict()

    # get all stores from crash_plan
    # (1) crash_plan itself
    # (2) all stores in the same cacheline happen before it
    def get_stores_from_crash_plan(self, crash_plan):
        cacheline = self.get_cacheline_from_address(crash_plan.address)
        return cacheline.get_stores_from_crash_plan(crash_plan)

    # get the corresponding cacheline from an address , create one if not having
    def get_cacheline_from_address(self, address):
        cacheline_address = get_cacheline_address(address)
        if cacheline_address not in self.cacheline_dict:
            cacheline = WitcherCacheline(cacheline_address,
                                         CACHELINE_BYTES,
                                         self.binary_file)
            self.cacheline_dict[cacheline_address] = cacheline
        return self.cacheline_dict[cacheline_address]

    def get_cachelines(self):
        return self.cacheline_dict.values()
