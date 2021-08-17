import engines.replayengines
from mem.memoryoperations import Store
from mem.memoryoperations import Flush
from mem.memoryoperations import Fence
from mem.memoryoperations import TXStart
from mem.memoryoperations import TXEnd
from mem.memoryoperations import PMDKTXAdd
from mem.memoryoperations import PMDKTXAlloc
from mem.memoryoperations import PMDKCall
from mem.cachenumbers import ATOMIC_WRITE_BYTES
from misc.witcherexceptions import NotSupportedOperationException
from logging import getLogger

class WitcherTrace:
    def __init__(self, arg_trace):
        # unprocessed trace
        self.trace = open(arg_trace).read().split("\n")
        self.trace = self.trace[:-1]
        # process the trace
        self.ops_tx_ranges = []
        self.atomic_write_ops_tx_ranges = []
        self.ops, self.atomic_write_ops = self.extract_operations()

        getLogger().debug("ops: " + str(self.ops))
        getLogger().debug("ops_tx_ranges: " + str(self.ops_tx_ranges))
        getLogger().debug("atomic_write_ops: " + str(self.atomic_write_ops))
        getLogger().debug("atomic_write_ops_tx_ranges: " + \
                          str(self.atomic_write_ops_tx_ranges))

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
                getLogger().debug("Store addr: " + hex(atomic_store.address) +
                                "; Store size: " + str(atomic_store.size) +
                                "; Store value: " + str(atomic_store.value))
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
                getLogger().debug("Store addr: " + hex(atomic_store.address) +
                                "; Store size: " + str(atomic_store.size) +
                                "; Store value: " + str(atomic_store.value))
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
                getLogger().debug("Store addr: " + hex(atomic_store.address) +
                                "; Store size: " + str(atomic_store.size) +
                                "; Store value: " + str(atomic_store.value))

            return store, atomic_store_list

        ops = []
        atomic_write_ops = []
        getLogger().debug("Trace: " + str(self.trace))

        # scan the trace and process each by its type
        op_curr_index = 0
        op_curr_tx_start_index = -1
        atomic_write_op_curr_index = 0
        atomic_write_op_curr_tx_start_index = -1
        for entry in self.trace:
            getLogger().debug("op_index:%d, atomic_op_index:%d, entry:%d",
                              op_curr_index, atomic_write_op_curr_index, entry)
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

                assert(op_curr_tx_start_index == -1)
                op_curr_tx_start_index = op_curr_index
                assert(atomic_write_op_curr_tx_start_index == -1)
                atomic_write_op_curr_tx_start_index = atomic_write_op_curr_index
            elif op_type == "TXEnd":
                tid = int(strs[1], 10)
                src_info = strs[2]
                ops.append(TXEnd(op_curr_index, tid, src_info))
                atomic_write_ops.append(TXEnd(atomic_write_op_curr_index,
                                              tid,
                                              src_info))

                assert(op_curr_tx_start_index != -1)
                self.ops_tx_ranges.append([op_curr_tx_start_index,
                                           op_curr_index])
                op_curr_tx_start_index = -1
                assert(atomic_write_op_curr_tx_start_index != -1)
                self.atomic_write_ops_tx_ranges.append([
                                            atomic_write_op_curr_tx_start_index,
                                            atomic_write_op_curr_index])
                atomic_write_op_curr_tx_start_index = -1
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
                note = strs[3]
                ops.append(PMDKCall(op_type, op_curr_index, tid, src_info, note))
                atomic_write_ops.append(PMDKCall(op_type,
                                                 atomic_write_op_curr_index,
                                                 tid,
                                                 src_info,
                                                 note))

            op_curr_index = op_curr_index + 1
            atomic_write_op_curr_index = atomic_write_op_curr_index + 1

        return ops, atomic_write_ops
