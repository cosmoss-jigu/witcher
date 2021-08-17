import os
import pickle
from mem.memoryoperations import Store, Fence, TXEnd
from mem.cachenumbers import get_cacheline_address
from engines.witcher.binaryfile import BinaryFile
from engines.witcher.pmdktracehandler import PMDKTraceHandler
from engines.witcher.witchercache import WitcherCache
from engines.witcher.witchercrashvalidator import WitcherCrashValidator
from engines.witcherparallel.witcherparallelengine import PICKLE_TRACE
from engines.witcherparallel.witcherparallelserver import SocketClient
from engines.witcherparallel.witcherparallelserver import SocketCmdCrashValidateRes

class WitcherParallelCrashManager:
    def __init__(self, tx_id, pre_store_id, suc_store_id, args):
        # initialize self fields for components
        self.init_misc(tx_id, pre_store_id, suc_store_id, args)
        # initialize trace
        self.init_trace()
        # initialize cache
        self.init_cache()
        # initialize crash validate
        self.init_crash_validator()
        # initialize crash target
        self.init_crash_target()

    def run(self):
        # replay from beginning to target store without crash
        self.run_before_target()
        # replay from target store
        # and try to find right places to crash and validate
        self.run_try_traget()
        # send results to the server
        self.epilogue()

    # initialize self fields for components
    def init_misc(self, tx_id, pre_store_id, suc_store_id, args):
        # initialize crash target info
        self.tx_id = tx_id
        self.pre_store_id = pre_store_id
        self.suc_store_id = suc_store_id

        # initialize output
        self.output = args.output_dir + '/' + \
                  str(tx_id) + '-' + str(pre_store_id) + '-' + str(suc_store_id)
        os.system('mkdir ' + self.output)

        # initialize for binary file
        self.pmdk_mmap_file = args.pmdk_mmap_file
        self.pmdk_mmap_size = args.pmdk_mmap_size
        self.replay_pm_file = self.output + '/' + self.pmdk_mmap_file
        with open(self.replay_pm_file, "w") as out:
            out.truncate(int(self.pmdk_mmap_size) * 1024 * 1024)
            out.close()

        # initialize for pmdk trace handler
        self.pmdk_mmap_base_addr = args.pmdk_mmap_base_addr
        self.pmdk_op_tracefile = args.pmdk_op_tracefile
        self.pmdk_val_tracefile = args.pmdk_val_tracefile

        # initialize for crash validate
        self.validate_exe = args.validate_exe
        self.pmdk_create_layout = args.pmdk_create_layout
        self.validate_op_file = args.validate_op_file
        self.full_oracle_file = args.full_oracle_file

    # initialize trace
    def init_trace(self):
        trace_path = self.output + '/../' + PICKLE_TRACE
        self.trace = pickle.load(open(trace_path, 'rb'))

    # initialize cache
    def init_cache(self):
        self.binary_file = BinaryFile(self.replay_pm_file, \
                                      self.pmdk_mmap_base_addr)
        self.pmdk_trace_handler = PMDKTraceHandler(self.binary_file,
                                                   self.pmdk_op_tracefile,
                                                   self.pmdk_val_tracefile)
        self.cache = WitcherCache(self.binary_file, self.pmdk_trace_handler)

    # initialize crash validate
    def init_crash_validator(self):
        self.crash_validator = WitcherCrashValidator(self.cache,
                                                     self.validate_exe,
                                                     self.replay_pm_file,
                                                     self.pmdk_mmap_base_addr,
                                                     self.pmdk_mmap_size,
                                                     self.pmdk_create_layout,
                                                     self.validate_op_file,
                                                     self.full_oracle_file,
                                                     self.output)
    # initialize crash target
    def init_crash_target(self):
        self.tx_range = self.trace.atomic_write_ops_tx_ranges[self.tx_id]
        self.pre_store = self.trace.atomic_write_ops[self.pre_store_id]
        assert(self.pre_store.id == self.pre_store_id and \
                isinstance(self.pre_store, Store))
        self.suc_store = self.trace.atomic_write_ops[self.suc_store_id]
        assert(self.suc_store.id == self.suc_store_id and \
                isinstance(self.suc_store, Store))

    # replay from beginning to target store without crash
    def run_before_target(self):
        max_id = max(self.pre_store_id, self.suc_store_id)
        ops = self.trace.atomic_write_ops[0:max_id+1]
        for op in ops:
            if self.cache.accept(op):
                # write back at fence
                self.cache.write_back_all_flushing_stores()
                continue
            if isinstance(op, TXEnd):
                # write back at TXEnd
                self.cache.write_back_all_flushing_stores()
                continue

    # replay from target store
    # and try to find right places to crash and validate
    def run_try_traget(self):
        max_id = max(self.pre_store_id, self.suc_store_id)
        ops = self.trace.atomic_write_ops[max_id+1:self.tx_range[1]+1]
        for op in ops:
            if self.cache.accept(op):
                # try crash at fence
                self.try_crash(op)
                # write back at fence
                self.cache.write_back_all_flushing_stores()
                # exit condition:
                # if suc_store is fenced, there nothing we can do
                if self.suc_store.is_fenced():
                    return
        # TODO: missing flushes
        # write back at TXEnd
        self.cache.write_back_all_stores()

    # check whether it is time to crash
    # if it is we crash and validate
    def try_crash(self, fence_op):
        crash_plans = []

        p_store = self.pre_store
        v_store = self.suc_store
        assert(p_store.id != v_store.id)
        assert(p_store.id != fence_op.id)
        assert(v_store.id != fence_op.id)
        assert(max(p_store.id, v_store.id) < fence_op.id)

        # if both of them are not fenced (volatile in the cache)
        if not p_store.is_fenced() and not v_store.is_fenced():
            # if the p_store happens first, no matter whether they are in
            # the same cacheline, we are able to only persist p_store and
            # leave v_store volatile
            if p_store.id < v_store.id:
                crash_plans.append(p_store)
            # if the v_store happens first, only do it when they are in
            # different cachelines
            else:
                if get_cacheline_address(p_store.address) != \
                        get_cacheline_address(v_store.address):
                    crash_plans.append(p_store)

        # if p_store is already persisted
        if p_store.is_fenced():
            # p_store and v_store cannot be both fenced here
            # because the last fence already processed the candidate
            assert(not v_store.is_fenced())
            # If a crash plan is a fence op, it means persisting nothing
            crash_plans.append(fence_op)

        # can have at most only one of the above cases
        assert(len(crash_plans) <= 1)
        # crash and validate
        # if crash_plans is empty, the following will return immediately
        self.crash_validator.validate(self.tx_id,
                                      fence_op.id,
                                      crash_plans)

    # send results to the server
    def epilogue(self):
        # initialize a socket client
        socket_client = SocketClient()
        v = self.crash_validator
        socket_cmd = SocketCmdCrashValidateRes(v.tested_crash_plans,
                                               v.reported_crash_plans,
                                               v.reported_src_map,
                                               v.reported_core_dump_map,
                                               v.reported_priority)
        socket_client.send_socket_cmd(socket_cmd)
