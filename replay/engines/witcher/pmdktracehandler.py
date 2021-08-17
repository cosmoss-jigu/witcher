import misc.utils as utils
from logging import getLogger

# a PMDK flush
# base_off is the offset for binary file inferred from register file mapping
class PMDKFlush:
    def __init__(self, addr, base_off, size, value):
        self.addr = addr
        self.base_off = base_off
        self.size = size
        self.value = value

class PMDKTraceHandler:
    def __init__(self, binary_file, pmdk_op_tracefile, pmdk_val_tracefile):
        # binary file for writing
        self.binary_file = binary_file

        # pmdk op trace and index
        pmdk_op_trace = open(pmdk_op_tracefile).read().split('\n')
        self.pmdk_op_trace = pmdk_op_trace[:-1]
        self.pmdk_op_trace_index = 0;

        # pmdk val trace and index
        self.pmdk_val_trace= utils.memory_map(pmdk_val_tracefile)
        self.pmdk_val_trace_index = 0;

        # a set of register mapping
        # (addr, size)
        self.register_set = set()

    # accept a pmdk call and replay its corresponding trace
    def accept(self, pmdk_call_op):
        # get the flush list of the pmdk call
        flush_list = self.get_flush_list(pmdk_call_op.func_name)

        # apply those flushes
        for flush in flush_list:
            self.binary_file.do_store_direct(flush.base_off,
                                             flush.base_off + flush.size,
                                             flush.value)

    def get_flush_list(self, func_name):
        # a function call stack
        func_stack = list()
        # the flush list will be returned
        flush_list = list()

        # traverse from current index of op trace
        while self.pmdk_op_trace_index < len(self.pmdk_op_trace):
            op = self.pmdk_op_trace[self.pmdk_op_trace_index].split(';')
            # process each op
            self.process_op(op, func_name, func_stack, flush_list)
            # if the stack is empty, we update the index and return
            if len(func_stack) == 0:
                self.pmdk_op_trace_index = self.pmdk_op_trace_index + 1
                return flush_list
            self.pmdk_op_trace_index = self.pmdk_op_trace_index + 1

        # should never come to here
        assert(False)

    # identify each op and assign to the corresponding function
    def process_op(self, op, func_name, func_stack, flush_list):
        op_type = op[0]
        if op_type == 'BEGIN':
            self.process_op_begin(op, func_name, func_stack)
        elif op_type == 'END':
            self.process_op_end(op, func_stack)
        elif op_type == 'REGISTER_FILE':
            self.process_op_register(op)
        elif op_type == 'FLUSH':
            self.process_op_flush(op, flush_list)
        else:
            raise NotSupportedOperationException(op)

    # push to function stack
    def process_op_begin(self, op, func_name, func_stack):
        getLogger().debug("Push:" + op[1])
        op_func_name = op[1]
        if len(func_stack) == 0:
            # the first op func should match the func we are replaying for
            if func_name == 'pmemobj_tx_add_range':
                assert(op_func_name == 'pmemobj_tx_add_range' or \
                       op_func_name == 'pmemobj_tx_add_range_direct')
            elif func_name == 'pmemobj_tx_alloc':
                assert(op_func_name == 'pmemobj_tx_alloc' or \
                       op_func_name == 'pmemobj_tx_zalloc')
            elif func_name == 'pmemobj_tx_end':
                assert(op_func_name == 'pmemobj_tx_commit')
            else:
                assert(op_func_name == func_name)
        func_stack.append(op_func_name)

    # pop form function stack
    def process_op_end(self, op, func_stack):
        getLogger().debug("Pop:" + op[1])
        # the stack should not be empty
        assert(len(func_stack) > 0)
        op_func_name = op[1]
        pop_func_name = func_stack.pop()
        # function name should match
        assert(op_func_name == pop_func_name)

    # register a memory mapping
    # TODO: we assume only one mapping file now, and we ignore offset
    def process_op_register(self, op):
        addr = int(op[2], 16)
        size = int(op[3], 16)
        offset = int(op[4], 16)
        assert(offset == 0)

        register_mapping = (addr, size)
        if register_mapping not in self.register_set:
            self.register_set.add(register_mapping)

    # a flush op
    def process_op_flush(self, op, flush_list):
        addr = int(op[1], 16)
        size = int(op[2], 16)
        # get the base_off based on the register mapping
        base_off = self.get_binary_base_off(addr, size)

        # get the value form val trace and update its index
        value = self.pmdk_val_trace[self.pmdk_val_trace_index: \
                                    self.pmdk_val_trace_index+size]
        self.pmdk_val_trace_index = self.pmdk_val_trace_index+size

        # update the flush list
        flush_list.append(PMDKFlush(addr, base_off, size, value))

    # get the base_off based on the register mapping
    def get_binary_base_off(self, addr, size):
        # traverse all the set and get the correct mapping
        for register_mapping in self.register_set:
            if addr >= register_mapping[0] and \
                addr + size <= register_mapping[0] + register_mapping[1]:
               return addr - register_mapping[0]
        # never come to here
        assert(False)
