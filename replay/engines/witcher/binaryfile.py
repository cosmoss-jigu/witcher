import misc.utils as utils
from logging import getLogger

class BinaryFile:

    def __init__(self, file_name, map_base):
        self.file_name = file_name
        self.map_base = int(map_base, 16)

        self._file_map = utils.memory_map(file_name)

    def __str__(self):
        return self.file_name

    # Write using a store op
    def do_store(self, store_op):
        base_off = store_op.get_base_address() - self.map_base
        max_off = store_op.get_max_address() - self.map_base
        self._file_map[base_off:max_off] = store_op.value_bytes
        # only flush before crash
        # self._file_map.flush(base_off & ~4095, 4096)

        getLogger().debug(
                 "file_name:%s, store_op:%d, base_off:%s, max_off:%s, value:%s",
                 self.file_name, store_op.id, hex(base_off), hex(max_off),
                 store_op.value_bytes.hex())

    # write using base_off, max_off and val, this is used by PMDK Trace Handler
    def do_store_direct(self, base_off, max_off, val):
        self._file_map[base_off:max_off] = val
        # only flush before crash
        # self._file_map.flush(base_off & ~4095, 4096)

    # only flush before crash
    def flush(self):
        self._file_map.flush()
