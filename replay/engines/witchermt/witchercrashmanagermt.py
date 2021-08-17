from engines.witcher.witchercrashmanager import CrashCandidates
from engines.witcher.witchercrashmanager import WitcherCrashManager
from mem.memoryoperations import Store, Fence, TXEnd

# a fake tx id for place holder
# we already know we should crash inside parallel ops
FAKE_TX_ID = 0

class CrashCandidatesMt(CrashCandidates):
    def __init__(self, trace, belief_database):
        self.trace = trace
        self.belief_database = belief_database

        # init the candidates
        self.get_crash_candidates()

    def get_crash_candidates(self):
        # get parallel ops
        ops = self.trace.get_parallel_ops()
        stores = list(filter(lambda op: isinstance(op, Store), ops))
        # get crash candidates of parallel ops
        self.crash_candidates = self.get_crash_candidates_from_tx(stores)
        # used by the WitcherCrashManager
        self.crash_candidates_per_tx = [self.crash_candidates]

class WitcherCrashManagerMt(WitcherCrashManager):
    def __init__(self,
                 trace,
                 belief_database,
                 cache,
                 crash_validator):
        self.trace = trace
        self.cache = cache
        self.crash_validator = crash_validator

        # get crash candidates
        self.crash_candidates = CrashCandidatesMt(trace, belief_database)

    def run(self):
        # replay from beginning to beginning of the parallel ops without crash
        self.run_before_parallel_ops()
        # replay from the beginning of the parallel ops
        # and try to find right places to crash and validate
        self.run_try_parallel_ops()

    # replay from beginning to target store without crash
    def run_before_parallel_ops(self):
        ops = self.trace.get_prefix_ops()
        for op in ops:
            self.cache.accept(op)
        # simply flush all before the parallel ops
        self.cache.write_back_all_stores()

    # replay from the beginning of the parallel ops
    # and try to find right places to crash and validate
    def run_try_parallel_ops(self):
        ops = self.trace.get_parallel_ops()
        for op in ops:
            if self.cache.accept(op):
                # try crash at fence
                processed_candidates = \
                                self.try_crash_candidates(FAKE_TX_ID, op.id, op)
                # if it is fence then write back all flushing stores
                self.cache.write_back_all_flushing_stores()
                # bring back potential candidates
                self.bring_back_potential_candidates(processed_candidates,\
                                                     FAKE_TX_ID)
        # TODO: missing flushes
        # write back all stores at the end of TX
        self.cache.write_back_all_stores()
