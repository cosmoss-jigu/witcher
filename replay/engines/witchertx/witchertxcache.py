from misc.witcherexceptions import NotSupportedOperationException
from mem.memoryoperations import Store
from mem.memoryoperations import Flush
from mem.memoryoperations import Fence
from mem.memoryoperations import TXStart
from mem.memoryoperations import TXEnd
from mem.memoryoperations import PMDKTXAdd
from mem.memoryoperations import PMDKTXAlloc
from mem.memoryoperations import PMDKCall

class WitcherTxCache:
    def __init__(self, binary_file, pmdk_trace_handler):
        # binary file for writing
        self.binary_file = binary_file
        # handle pmdk trace
        self.pmdk_trace_handler = pmdk_trace_handler

        # a stack for pmdk tx begin, since it can be nested
        self.pmdk_tx_stack = 0
        self.store_list = []

    def accept(self, op):
        if isinstance(op, Store):
            self.accept_store(op)
        elif isinstance(op, Flush):
            self.accept_flush(op)
        elif isinstance(op, Fence):
            self.accept_fence(op)
        elif isinstance(op, PMDKTXAdd):
            self.pmdk_trace_handler.accept(op)
        elif isinstance(op, PMDKTXAlloc):
            self.pmdk_trace_handler.accept(op)
        elif isinstance(op, PMDKCall):
            if op.func_name == 'pmemobj_tx_begin':
                assert(self.pmdk_tx_stack >= 0)
                self.pmdk_tx_stack = self.pmdk_tx_stack + 1
            elif op.func_name == 'pmemobj_tx_end':
                assert(self.pmdk_tx_stack > 0)
                self.pmdk_tx_stack= self.pmdk_tx_stack - 1
                self.pmdk_trace_handler.accept(op)
            else:
                self.pmdk_trace_handler.accept(op)
        elif isinstance(op, TXStart) or isinstance(op, TXEnd):
            pass
        else:
            raise NotSupportedOperationException(op)

    # accept_store
    def accept_store(self, store_op):
        # we only trace stores outside pmdk tx
        if self.pmdk_tx_stack == 0:
            self.store_list.append(store_op)
        # if it is inside a pmdk tx, we directly persist it
        else:
            self.binary_file.do_store(store_op)

    # accept_flush
    def accept_flush(self, flush_op):
        assert(self.pmdk_tx_stack == 0)

    # accept_fence
    # we assume all flush and fence are outside pmdk tx and they are all correct
    def accept_fence(self, flush_op):
        assert(self.pmdk_tx_stack == 0)
        # Write all stores in the cache to the file
        for store_op in self.store_list:
            self.binary_file.do_store(store_op)
        # Clear the store_list
        self.stores_list = []
