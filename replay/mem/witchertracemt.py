from mem.memoryoperations import Store
from mem.memoryoperations import Flush
from mem.memoryoperations import Fence
from mem.memoryoperations import TXStart
from mem.memoryoperations import TXEnd
from mem.memoryoperations import PMDKTXAdd
from mem.memoryoperations import PMDKTXAlloc
from mem.memoryoperations import PMDKCall
from mem.cachenumbers import ATOMIC_WRITE_BYTES

class WitcherTraceMt:
    def __init__(self, arg_trace, parallel_start_index, num_threads):
        # unprocessed trace
        self.trace = open(arg_trace).read().split("\n")
        self.trace = self.trace[:-1]

        # parallel index and range
        self.parallel_start_index = parallel_start_index
        self.num_threads = num_threads
        self.atomic_write_ops_parallel_range = [-1, -1]

        # process the trace
        self.ops, self.atomic_write_ops = self.extract_operations()

    # get prefix ops before parallel ops
    def get_prefix_ops(self):
        return self.atomic_write_ops[0:self.atomic_write_ops_parallel_range[0]]

    # get parallel ops
    def get_parallel_ops(self):
        return self.atomic_write_ops[self.atomic_write_ops_parallel_range[0]:\
                                     self.atomic_write_ops_parallel_range[1]+1]

    def extract_operations(self):
        # split a store entry in ATOMIC_WRITE_BYTES size
        def get_stores(params, op_curr_index, atomic_write_op_curr_index):
            tid = int(params[1], 10)
            address = int(params[2], 16)
            size = int(params[3], 10)
            value = params[4].split(" ")
            src_info = params[5]

            store = Store(address, size, value, op_curr_index, tid, src_info)

            atomic_store_list = []

            address_atomic_write_low = int(address/ATOMIC_WRITE_BYTES) * \
                                       ATOMIC_WRITE_BYTES
            address_atomic_write_high = address_atomic_write_low + \
                                        ATOMIC_WRITE_BYTES
            if address > address_atomic_write_low:
                size_first_chunk = \
                                  min(size, address_atomic_write_high - address)
                atomic_store = Store(address,
                                     size_first_chunk,
                                     value[0:size_first_chunk],
                                     atomic_write_op_curr_index,
                                     tid,
                                     src_info)
                atomic_write_op_curr_index = atomic_write_op_curr_index + 1
                atomic_store_list.append(atomic_store)
                curr_offset = address_atomic_write_high - address
            else:
                curr_offset = 0
            while curr_offset + ATOMIC_WRITE_BYTES <= size:
                atomic_store = Store(address+curr_offset,
                                     ATOMIC_WRITE_BYTES,
                                     value[curr_offset:curr_offset+ATOMIC_WRITE_BYTES],
                                     atomic_write_op_curr_index,
                                     tid,
                                     src_info)
                atomic_write_op_curr_index = atomic_write_op_curr_index + 1
                atomic_store_list.append(atomic_store)
                curr_offset += ATOMIC_WRITE_BYTES

            if size - curr_offset > 0:
                atomic_store = Store(address+curr_offset,
                                     size-curr_offset,
                                     value[curr_offset:],
                                     atomic_write_op_curr_index,
                                     tid,
                                     src_info)
                atomic_write_op_curr_index = atomic_write_op_curr_index + 1
                atomic_store_list.append(atomic_store)

            return store, atomic_store_list

        ops = []
        atomic_write_ops = []

        # scan the trace and process each by its type
        op_curr_index = 0
        atomic_write_op_curr_index = 0
        tx_start_curr_index = 0
        tx_end_curr_index = 0
        for entry in self.trace:
            strs = entry.split(",")
            op_type = strs[0]
            if op_type == "Store":
                store, atomic_store_list = get_stores(strs,
                                                      op_curr_index,
                                                      atomic_write_op_curr_index)
                ops.append(store)
                atomic_write_ops += atomic_store_list
                atomic_write_op_curr_index = atomic_write_op_curr_index \
                                             + len(atomic_store_list) \
                                             - 1
            elif op_type == "Flush":
                tid = int(strs[1], 10)
                address = int(strs[2], 16)
                src_info = strs[3]
                ops.append(Flush(address, op_curr_index, tid, src_info))
                atomic_write_ops.append(Flush(address,
                                              atomic_write_op_curr_index,
                                              tid,
                                              src_info))
            elif op_type == "Fence":
                tid = int(strs[1], 10)
                src_info = strs[2]
                ops.append(Fence(op_curr_index, tid, src_info))
                atomic_write_ops.append(Fence(atomic_write_op_curr_index,
                                              tid,
                                              src_info))
            elif op_type == "TXStart":
                tid = int(strs[1], 10)
                src_info = strs[2]
                ops.append(TXStart(op_curr_index, tid, src_info))
                atomic_write_ops.append(TXStart(atomic_write_op_curr_index,
                                                tid,
                                                src_info))
                # identify parallel range
                if tx_start_curr_index == self.parallel_start_index:
                    self.atomic_write_ops_parallel_range[0] = \
                                                     atomic_write_op_curr_index
                tx_start_curr_index += 1
            elif op_type == "TXEnd":
                tid = int(strs[1], 10)
                src_info = strs[2]
                ops.append(TXEnd(op_curr_index, tid, src_info))
                atomic_write_ops.append(TXEnd(atomic_write_op_curr_index,
                                              tid,
                                              src_info))
                # identify parallel range
                if tx_end_curr_index == self.parallel_start_index + \
                                        self.num_threads - 1:
                    self.atomic_write_ops_parallel_range[1] = \
                                                     atomic_write_op_curr_index
                tx_end_curr_index += 1
            elif op_type == "TXAlloc":
                tid = int(strs[1], 10)
                address = int(strs[2], 16)
                size = int(strs[3], 10)
                src_info = strs[4]
                ops.append(PMDKTXAlloc(address, size, op_curr_index, tid, src_info))
                atomic_write_ops.append(PMDKTXAlloc(address,
                                                    size,
                                                    atomic_write_op_curr_index,
                                                    tid,
                                                    src_info))
            elif op_type == "TXAdd":
                tid = int(strs[1], 10)
                address = int(strs[2], 16)
                size = int(strs[3], 10)
                src_info = strs[4]
                ops.append(PMDKTXAdd(address, size, op_curr_index, tid, src_info))
                atomic_write_ops.append(PMDKTXAdd(address,
                                                  size,
                                                  atomic_write_op_curr_index,
                                                  tid,
                                                  src_info))
            else:
                tid = int(strs[1], 10)
                src_info = strs[2]
                ops.append(PMDKCall(op_type, op_curr_index, tid, src_info))
                atomic_write_ops.append(PMDKCall(op_type,
                                                 atomic_write_op_curr_index,
                                                 tid,
                                                 src_info))

            op_curr_index = op_curr_index + 1
            atomic_write_op_curr_index = atomic_write_op_curr_index + 1

        return ops, atomic_write_ops
