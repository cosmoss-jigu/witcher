import os
import pickle
from mem.memoryoperations import Store, Fence, TXStart, TXEnd
from mem.cachenumbers import get_cacheline_address
from engines.witcher.binaryfile import BinaryFile
from engines.witcher.pmdktracehandler import PMDKTraceHandler
from engines.witcher.witchercache import WitcherCache
from engines.witcher.witchercrashvalidator import WitcherCrashValidator
from engines.witcher.witchercrashmanager import CrashCandidates
from engines.witcher.witchercrashmanager import WitcherCrashManager
from engines.witcherparallel.witcherparallelengine import PICKLE_TRACE
from engines.witcherparallel.witcherparallelengine import PICKLE_BELIEF
from engines.witcherparallel.witcherparallelengine import PICKLE_VALIDATE_RES


# get crash candidates for a witcher tx
class CrashCandidatesParallel(CrashCandidates):
    def __init__(self, trace, belief_database, tx_id):
        self.trace = trace
        self.belief_database = belief_database
        self.tx_id = tx_id

        # init the candidates
        self.get_crash_candidates_from_tx_id()

    def get_crash_candidates_from_tx_id(self):
        tx_range = self.trace.atomic_write_ops_tx_ranges[self.tx_id]
        # all ops in this tx
        ops = self.trace.atomic_write_ops[tx_range[0]:tx_range[1]+1]
        # all stores in this tx
        stores = list(filter(lambda op: isinstance(op, Store), ops))
        # init candidates of this tx
        self.crash_candidates = self.get_crash_candidates_from_tx(stores)
        # used by the WitcherCrashManager
        self.crash_candidates_per_tx = dict()
        self.crash_candidates_per_tx[self.tx_id] = self.crash_candidates

class WitcherParallelCrashManager(WitcherCrashManager):
    def __init__(self, args, tx_id):
        # initialize self fields for components
        self.init_misc_0(args, tx_id)
        # initialize trace
        self.init_trace()

        # TODO if too long, we just return
        curr_range = self.trace.atomic_write_ops_tx_ranges[self.tx_id]
        if (curr_range[1] - curr_range[0] > 90000):
            return

        # initialize belief_database
        self.init_belief_database()
        # initialize crash candidates
        self.init_crash_candidates()

        self.crash_max = int(args.crash)

        # if there is no crash_candidates, we just return
        if len(self.crash_candidates.crash_candidates) == 0:
            return
        # TODO if too many crash_candidates, we just return
        if len(self.crash_candidates.crash_candidates) > self.crash_max:
            return

        # initialize self fields for components
        self.init_misc_1(args, tx_id)
        # initialize cache
        self.init_cache()
        # initialize crash validate
        self.init_crash_validator()

    def run(self):
        # TODO if too long, we just return
        curr_range = self.trace.atomic_write_ops_tx_ranges[self.tx_id]
        if (curr_range[1] - curr_range[0] > 90000):
            return
        # if there is no crash_candidates, we just return
        if len(self.crash_candidates.crash_candidates) == 0:
            return
        # TODO if too many crash_candidates, we just return
        if len(self.crash_candidates.crash_candidates) > self.crash_max:
            return

        # replay from beginning to target store without crash
        self.run_before_target()
        # replay from target store
        # and try to find right places to crash and validate
        self.run_try_traget()
        # send results to the server
        self.epilogue()

    # initialize self fields for components
    def init_misc_0(self, args, tx_id):
        self.tx_id = tx_id
        self.output_partent = args.output_dir
        self.server_name = args.server_name

    # initialize self fields for components
    def init_misc_1(self, args, tx_id):
        # initialize output
        self.output = args.output_dir + '/tx-' + str(tx_id)
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

        # initialize for crash validate
        self.validate_exe = args.validate_exe
        self.pmdk_create_layout = args.pmdk_create_layout
        self.validate_op_file = args.validate_op_file
        self.full_oracle_file = args.full_oracle_file

    # initialize trace
    def init_trace(self):
        trace_path = self.output_partent + '/' + PICKLE_TRACE
        self.trace = pickle.load(open(trace_path, 'rb'))

    # initialize belief database
    def init_belief_database(self):
        belief_database_path = self.output_partent + '/' + PICKLE_BELIEF
        self.belief_database = pickle.load(open(belief_database_path, 'rb'))

    # initialize crash candidates
    def init_crash_candidates(self):
        self.crash_candidates =  CrashCandidatesParallel(self.trace,
                                                         self.belief_database,
                                                         self.tx_id)
        self.crash_candidates_snapshot = self.crash_candidates.crash_candidates.copy()

    # initialize cache
    def init_cache(self):
        self.binary_file = BinaryFile(self.replay_pm_file, \
                                      self.pmdk_mmap_base_addr)
        self.cache = WitcherCache(self.binary_file)

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
                                                     self.output,
                                                     self.server_name,
                                                     self.tx_id)

    # replay from beginning to target store without crash
    def run_before_target(self):
        first_tx_start_id = self.trace.atomic_write_ops_tx_ranges[0][0]
        target_tx_range = self.trace.atomic_write_ops_tx_ranges[self.tx_id]
        ops = self.trace.atomic_write_ops[0:target_tx_range[0]]
        for op in ops:
            # if it is fence
            if self.cache.accept(op):
                # if it is fence then write back all flushing stores
                self.cache.write_back_all_flushing_stores()
            # if it is TXEnd then write back all stores
            if isinstance(op, TXEnd):
                self.cache.write_back_all_stores()
            # if it is the one right before the first TXStart
            # then write back all stores
            if op.id == first_tx_start_id - 1:
                # write back at TXEnd
                self.cache.write_back_all_stores()

    # replay from target store
    # and try to find right places to crash and validate
    def run_try_traget(self):
        target_tx_range = self.trace.atomic_write_ops_tx_ranges[self.tx_id]
        ops = self.trace.atomic_write_ops[target_tx_range[0]:target_tx_range[1]]
        for op in ops:
            if self.cache.accept(op):
                # try crash at fence
                processed_candidates = \
                                self.try_crash_candidates(self.tx_id, op.id, op)
                # if it is fence then write back all flushing stores
                self.cache.write_back_all_flushing_stores()
                # bring back potential candidates
                self.bring_back_potential_candidates(processed_candidates,\
                                                     self.tx_id)
        # TODO: missing flushes
        # write back all stores at the end of TX
        self.cache.write_back_all_stores()

    # pickle the result
    def epilogue(self):
        v = self.crash_validator
        res = [v.tested_crash_plans, v.reported_crash_plans, \
               v.reported_src_map, v.reported_core_dump_map, \
               v.reported_priority, self.crash_candidates_snapshot]
        pickle.dump(res, open(self.output+'/'+PICKLE_VALIDATE_RES, 'wb'))
