from misc.utils import Rangeable
from misc.utils import range_cmp
from mem.cachenumbers import CACHELINE_BYTES, get_cacheline_address
from sys import byteorder
from logging import getLogger

class BaseOperation:
    def __init__(self, op_id, tid, src_info):
        self.id = op_id
        self.tid = tid
        self.src_info = src_info

    def __repr__(self):
        return "id:" + str(self.id) + ",tid:" + str(self.tid) + ",src_info:" + str(self.src_info)

    def __str__(self):
        return "id:" + str(self.id) + ",tid:" + str(self.tid) + ",src_info:" + str(self.src_info)

# TX Start Operation
class TXStart(BaseOperation):
    def __repr__(self):
        return "TXStart:" + BaseOperation.__repr__(self)

    def __str__(self):
        return "TXStart:" + BaseOperation.__str__(self)

# TX End Operation
class TXEnd(BaseOperation):
    def __repr__(self):
        return "TXEnd:" + BaseOperation.__repr__(self)

    def __str__(self):
        return "TXEnd:" + BaseOperation.__str__(self)

# PMDK Call Operation
class PMDKCall(BaseOperation):
    def __init__(self, func_name, op_id, tid, src_info, note):
        self.func_name = func_name
        self.note = note
        BaseOperation.__init__(self, op_id, tid, src_info)

    def __repr__(self):
        return self.func_name + ":" + self.note + ":" + BaseOperation.__repr__(self)

    def __str__(self):
        return self.func_name + ":" + self.note + ":" + BaseOperation.__repr__(self)

# PMDK TX_Add Operation
class PMDKTXAdd(BaseOperation, Rangeable):
    def __init__(self, addr, size, op_id, tid, src_info):
        self.address = addr
        self.size = size
        self.func_name = 'pmemobj_tx_add_range'
        BaseOperation.__init__(self, op_id, tid, src_info)

    def __repr__(self):
        return "PMDKTXadd:" + \
               "addr:" + hex(self.address) + \
               ",size:" + str(self.size) + \
               "," + BaseOperation.__str__(self)

    def __str__(self):
        return "PMDKTXadd:" + \
               "addr:" + hex(self.address) + \
               ",size:" + str(self.size) + \
               "," + BaseOperation.__str__(self)

    # Rangeable implementation
    def get_base_address(self):
        return self.address

    # Rangeable implementation
    def get_max_address(self):
        return self.address + self.size

# PMDK TX_Alloc Operation
class PMDKTXAlloc(BaseOperation, Rangeable):
    def __init__(self, addr, size, op_id, tid, src_info):
        self.address = addr
        self.size = size
        self.func_name = 'pmemobj_tx_alloc'
        BaseOperation.__init__(self, op_id, tid, src_info)

    def __repr__(self):
        return "PMDKTXAlloc:" + \
               "addr:" + hex(self.address) + \
               ",size:" + str(self.size) + \
               "," + BaseOperation.__str__(self)

    def __str__(self):
        return "PMDKTXAlloc:" + \
               "addr:" + hex(self.address) + \
               ",size:" + str(self.size) + \
               "," + BaseOperation.__str__(self)

    # Rangeable implementation
    def get_base_address(self):
        return self.address

    # Rangeable implementation
    def get_max_address(self):
        return self.address + self.size

# Fence Operation
class Fence(BaseOperation):
    def __repr__(self):
        return "Fence:" + BaseOperation.__repr__(self)

    def __str__(self):
        return "Fence:" + BaseOperation.__str__(self)

# Store Operation
class Store(BaseOperation, Rangeable):
    def __init__(self, address, size, value, op_id, tid, src_info):
        self.address = address
        self.size = size
        # flushed:
        # 0: clear
        # 1: mark it and to be flushed when fence
        # 2: already flushed and written back
        self.flushed = 0

        # Initialize the byte representation of value for writing to a file
        self.value = value
        value_bytes = ""
        for byte in value:
            value_bytes = value_bytes + byte
        self.value_bytes = bytes(bytearray.fromhex(value_bytes))

        BaseOperation.__init__(self, op_id, tid, src_info)

    def __repr__(self):
        return "Store:" + \
               "addr:" + hex(self.address) + \
               ",size:" + str(self.size) + \
               ",value:" + str(self.value) + \
               "," + BaseOperation.__str__(self)

    def __str__(self):
        return "Store:" + \
               "addr:" + hex(self.address) + \
               ",size:" + str(self.size) + \
               ",value:" + str(self.value) + \
               "," + BaseOperation.__str__(self)

    # Rangeable implementation
    def get_base_address(self):
        return self.address

    # Rangeable implementation
    def get_max_address(self):
        return self.address + self.size

    # mark it and to be flushed when fence
    def accept_flush(self):
        assert(self.is_clear() or self.is_flushing())
        if self.is_flushing():
            getLogger().debug("duplicated flush: " + str(self))
        self.flushed = 1

    # already flushed and written back
    def accept_fence(self):
        assert(self.is_clear() or self.is_flushing())
        if self.is_clear():
            getLogger().debug("missing flush: " + str(self))
        self.flushed = 2

    # check it is clear
    def is_clear(self):
        return self.flushed == 0

    # check it is to be flushed when fence
    def is_flushing(self):
        return self.flushed == 1

    # check it is fenced
    def is_fenced(self):
        return self.flushed == 2

# FlushBase interface
class FlushBase(BaseOperation, Rangeable):
    def is_in_flush(self, store_op):
        raise NotImplementedError

# Flush Operation
class Flush(FlushBase):
    def __init__(self, address_not_aligned, op_id, tid, src_info):
        self.address_not_aligned = address_not_aligned
        self.address = get_cacheline_address(address_not_aligned)
        self.size = CACHELINE_BYTES
        BaseOperation.__init__(self, op_id, tid, src_info)

    def __repr__(self):
        return "Flush:" + \
               "addr:" + hex(self.address) + \
               ",size:" + str(self.size) + \
               "," + BaseOperation.__str__(self)

    def __str__(self):
        return "Flush:" + \
               "addr:" + hex(self.address) + \
               ",size:" + str(self.size) + \
               "," + BaseOperation.__str__(self)

    # FlushBase implementation
    def is_in_flush(self, store_op):
        if range_cmp(store_op, self) == 0:
            return True
        else:
            return False

    # Rangeable implementation
    def get_base_address(self):
        return self.address

    # Rangeable implementation
    def get_max_address(self):
        return self.address + self.size
