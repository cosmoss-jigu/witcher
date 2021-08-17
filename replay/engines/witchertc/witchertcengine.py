from engines.replayengine import ReplayEngineBase
from mem.memoryoperations import TXStart, PMDKCall, PMDKTXAdd, PMDKTXAlloc, TXEnd, Store, Flush, Fence
from misc.utils import Rangeable, range_cmp
from mem.cachenumbers import CACHELINE_BYTES, ATOMIC_WRITE_BYTES, get_cacheline_address
from logging import getLogger
import os
import csv

class TCCache:
    def __init__(self):
        self.stores = []

    def accept_store(self, op):
        self.stores.append(op)

    def accept_flush(self, flush):
        for store in self.stores:
            if flush.is_in_flush(store):
                store.flushed = 1

    def accept_fence(self):
        self.stores = list(filter(lambda store: store.flushed == 0, self.stores))

    def is_extra_flush(self, flush):
        for store in self.stores:
            if flush.is_in_flush(store):
                if store.flushed == 0:
                    return False
        return True

    def is_extra_fence(self):
        for store in self.stores:
            if store.flushed == 1:
                return False
        return True

    def clear(self):
        self.stores.clear()

    def is_empty(self):
        return len(self.stores) == 0

    def get_all_stores(self):
        return self.stores;

    def get_flushing_stores(self):
        return list(filter(lambda store: store.flushed == 1, self.stores))

class CTRes:
    def __init__(self):
        self.unflushed_stores = set()
        self.extra_flushes = set()
        self.extra_fences = set()

        self.unlogged_stores = set()
        self.extra_logs = set()
        self.unTXed_stores = set()

    def print_res(self, path):
        self.print(path+'/unfluhsed_stores', self.unflushed_stores)
        self.print(path+'/extra_flushes', self.extra_flushes)
        self.print(path+'/extra_fences', self.extra_fences)

        self.print(path+'/unlogged_stores', self.unlogged_stores)
        self.print(path+'/extra_logs', self.extra_logs)
        self.print(path+'/unTXed_stores', self.unTXed_stores)

        self.print_summary(path+'/summary')

    def print(self, filename, res):
        d = dict()
        for entry in res:
            src_info = entry.src_info
            if src_info not in d:
                d[src_info] = []
            d[src_info].append(entry)

        with open(filename, 'w') as f:
            for key in d:
                f.write('KEY: ' + str(key))
                f.write('\n')
            f.write('\n')

            for key in d:
                f.write('KEY: ' + str(key))
                f.write('\n')
                for entry in d[key]:
                    f.write('\t')
                    f.write(str(entry))
                    f.write('\n')
                f.write('\n')

    def print_summary(self, filename):
        with open(filename, 'w') as f:
            f.write('unflushed_stores:' + str(len(self.unflushed_stores)))
            f.write('\n')
            f.write('extra_flushes:' + str(len(self.extra_flushes)))
            f.write('\n')
            f.write('extra_fences:' + str(len(self.extra_fences)))
            f.write('\n')

            f.write('unlogged_stores:' + str(len(self.unlogged_stores)))
            f.write('\n')
            f.write('extra_logs:' + str(len(self.extra_logs)))
            f.write('\n')
            f.write('unTXed_stores:' + str(len(self.unTXed_stores)))
            f.write('\n')

class WitcherTCEngine(ReplayEngineBase):
    # initialization
    def __init__(self):
        self.cache = TCCache()
        self.res = CTRes()

    # set the trace
    def set_trace(self, trace):
        self.trace = trace

    # traverse all the operations in the trace
    def run(self):
        ops = self.trace.atomic_write_ops
        witcher_tx_ranges = self.trace.atomic_write_ops_tx_ranges

        in_Witcher_TX = False

        use_PMDK_TX = False
        in_PMDK_TX = False
        pmdk_tx_stack = []
        pmdk_call_stack = []
        pmdk_logs = []

        # first run:
        # Outside TX
        #   (1) check unflushed at each TX_START, TX_END and the end of program
        #   (2) check extra flush at each flush
        #   (3) check extra fence at each fence
        # Inside TX
        #   (1) missing undo at each store
        #   (2) extra undo at each logging
        # also need to return whether it is TX_BASED
        for witcher_tx_range in witcher_tx_ranges:
            # # TODO if too long, we just return
            if witcher_tx_range[1] - witcher_tx_range[0] > 90000:
                continue
            for op in ops[witcher_tx_range[0]:witcher_tx_range[1]]:
                if isinstance(op, TXStart):
                    in_Witcher_TX = True
                    #for store in self.cache.get_all_stores():
                    #    self.res.unflushed_stores.add(store)

                elif isinstance(op, TXEnd):
                    in_Witcher_TX = False
                    #for store in self.cache.get_all_stores():
                    #    self.res.unflushed_stores.add(store)

                elif isinstance(op, PMDKCall):
                    func_name = op.func_name
                    note = op.note
                    if note == 'start':
                        pmdk_call_stack.append(func_name)
                    else:
                        assert(note == 'end')
                        popped = pmdk_call_stack.pop()
                        assert(popped == func_name)

                    if func_name == 'pmemobj_tx_begin' and note == 'end':
                        if in_PMDK_TX == False:
                            in_PMDK_TX = True
                            use_PMDK_TX = True
                        else:
                            pmdk_tx_stack.append(op)

                    if func_name == 'pmemobj_tx_end' and note == 'start':
                        assert(in_PMDK_TX == True)
                        if len(pmdk_tx_stack) > 0 :
                            pmdk_tx_stack.pop()
                        else:
                            in_PMDK_TX = False
                            pmdk_logs.clear()

                elif (isinstance(op, PMDKTXAdd) or isinstance(op, PMDKTXAlloc)):
                    assert(in_PMDK_TX == True)
                    for log in pmdk_logs:
                        if range_cmp(op, log) == 0:
                            self.res.extra_logs.add(op)
                            break
                    pmdk_logs.append(op)

                elif isinstance(op, Store):
                    self.cache.accept_store(op)

                    if in_PMDK_TX and len(pmdk_call_stack) == 0:
                        logged = False
                        for log in pmdk_logs:
                            if range_cmp(op, log) == 0:
                                logged = True
                                break
                        if not logged:
                            self.res.unlogged_stores.add(op)

                elif isinstance(op, Flush):
                    if self.cache.is_extra_flush(op):
                        self.res.extra_flushes.add(op)
                    self.cache.accept_flush(op)

                elif isinstance(op, Fence):
                    if self.cache.is_extra_fence():
                        self.res.extra_fences.add(op)
                    self.cache.accept_fence()

                else:
                    raise NotSupportedOperationException(op)

        for store in self.cache.get_all_stores():
            self.res.unflushed_stores.add(store)

        if not use_PMDK_TX:
            self.epilogue()
            return

        assert(in_PMDK_TX == False)
        assert(len(pmdk_tx_stack) == 0)
        assert(len(pmdk_call_stack) == 0)
        for witcher_tx_range in witcher_tx_ranges:
            # # TODO if too long, we just return
            if witcher_tx_range[1] - witcher_tx_range[0] > 90000:
                continue
            for op in ops[witcher_tx_range[0]:witcher_tx_range[1]]:
                if isinstance(op, PMDKCall):
                    func_name = op.func_name
                    note = op.note
                    if note == 'start':
                        pmdk_call_stack.append(func_name)
                    else:
                        assert(note == 'end')
                        popped = pmdk_call_stack.pop()
                        assert(popped == func_name)

                    if func_name == 'pmemobj_tx_begin' and note == 'end':
                        if in_PMDK_TX == False:
                            in_PMDK_TX = True
                        else:
                            pmdk_tx_stack.append(op)

                    if func_name == 'pmemobj_tx_end' and note == 'start':
                        assert(in_PMDK_TX == True)
                        if (len(pmdk_tx_stack) > 0):
                            pmdk_tx_stack.pop()
                        else:
                            in_PMDK_TX = False
                elif isinstance(op, Store):
                    if (not in_PMDK_TX) and len(pmdk_call_stack) == 0:
                        self.res.unTXed_stores.add(op)
                #elif isinstance(op, TXStart):
                #    pass
                #elif isinstance(op, TXEnd):
                #    pass
                #elif (isinstance(op, PMDKTXAdd) or isinstance(op, PMDKTXAlloc)):
                #    pass
                #elif isinstance(op, Flush):
                #    pass
                #elif isinstance(op, Fence):
                #    pass
                #else:
                #    raise NotSupportedOperationException(op)

        # second run:
        # find out stores outside pmdk_tx and pmdk_functions
        self.epilogue()

    def epilogue(self):
        path = 'tc'
        os.mkdir(path)
        self.res.print_res(path)

        f = open(path+'/atmoic_write.pmtrace', 'w')
        for op in self.trace.atomic_write_ops:
            f.write(str(op) + '\n')
