from mem.memoryoperations import Store, Fence, TXEnd
from mem.cachenumbers import get_cacheline_address
from logging import getLogger

# Crash Candidates which ignore cache simulation
# crashj_candidates_per_tx:
#   each tx has a list of ordered candidates
#   each candidate: [pre_store, suc_store]
#   pre_store.id < suc_store.id or pre_store.id > suc_store.id
#   persist pre_store and leave suc_store volatile
#   each list is ordered by max(pre_store.id, suc_store.id) from low to high
class CrashCandidates:
    def __init__(self):
        pass

    # init (calculate) candidates from belief
    def init_from_belief_database(self, trace, belief_database):
        self.trace = trace
        self.belief_database = belief_database

        # init the candidates
        self.crash_candidates_per_tx = []
        self.get_crash_candidates()

        #getLogger().debug("crash_candidates: " + \
        #                  str(self.crash_candidates_per_tx))

    # init candidates directly from a crash candidates file
    def init_from_crash_candidates(self, trace, crash_candidates_file):
        # init the candidates
        self.crash_candidates_per_tx = []

        curr_tx = -1
        cc = open(crash_candidates_file).read().split("\n")[:-1]
        for line in cc:
            entries = line.split('\t')
            assert(len(entries) == 2)
            if (entries[0] == 'tx'):
                curr_tx = curr_tx + 1
                assert(curr_tx == int(entries[1]))
                self.crash_candidates_per_tx.append([])
            else:
                pre_st = trace.atomic_write_ops[int(entries[0])]
                assert(isinstance(pre_st, Store) and \
                       pre_st.id == int(entries[0]))
                suc_st = trace.atomic_write_ops[int(entries[1])]
                assert(isinstance(suc_st, Store) and \
                       suc_st.id == int(entries[1]))
                self.crash_candidates_per_tx[curr_tx].append([pre_st, suc_st])

        #getLogger().debug("crash_candidates: " + \
        #                  str(self.crash_candidates_per_tx))

    def get_crash_candidates(self):
        # traverse each tx
        for tx_range in self.trace.atomic_write_ops_tx_ranges:
            # all ops in this tx
            ops = self.trace.atomic_write_ops[tx_range[0]:tx_range[1]+1]
            # all stores in this tx
            stores = list(filter(lambda op: isinstance(op, Store), ops))
            # init candidates of this tx
            crash_candidates = self.get_crash_candidates_from_tx(stores)
            self.crash_candidates_per_tx.append(crash_candidates)

    def get_crash_candidates_from_tx(self, stores):
        # TODO
        if len(stores) > 100000:
            return []
        crash_candidates = []
        # This traverse order guarantees the list order:
        # ordered by max(pre_store.id, suc_store.id) from low to high
        for suc_st_index in range(1, len(stores)):
            for pre_st_index in range(0, suc_st_index):
                pre_st = stores[pre_st_index]
                suc_st = stores[suc_st_index]

                # if it breaks aomicity,
                # then we don't do extra ordering belief check
                # this guarantees no duplicates in candidates
                if self.violate_atomicity_belief(pre_st, suc_st):
                    crash_candidates.append([pre_st, suc_st])
                    crash_candidates.append([suc_st, pre_st])
                    continue

                # try persist pre_st and leave suc_st volatile
                if self.violate_ordering_belief(pre_st, suc_st):
                    crash_candidates.append([pre_st, suc_st])

                # try persist suc_st and leave pre_st volatile
                if self.violate_ordering_belief(suc_st, pre_st):
                    crash_candidates.append([suc_st, pre_st])

        return crash_candidates

    def violate_atomicity_belief(self, pre_st, suc_st):
        # check whether they are both critical stores
        return self.belief_database.is_critical_store(pre_st) and \
               self.belief_database.is_critical_store(suc_st)

    def violate_ordering_belief(self, p_st, v_st):
        # Here we want to persist p_st and leave v_st volatile
        # which means we need to find a belief: v_st_obj -hb-> p_st_obj
        return self.belief_database.violate_ordering_belief(p_st, v_st)

class WitcherCrashManager:
    def __init__(self):
        pass

    # init from belief database
    def init_from_belief_database(self,
                                  trace,
                                  belief_database,
                                  cache,
                                  crash_validator):
        self.trace = trace
        self.cache = cache
        self.crash_validator = crash_validator

        # get crash candidates
        self.crash_candidates = CrashCandidates()
        self.crash_candidates.init_from_belief_database(trace, belief_database)

    # init from a crash candidates file
    def init_from_crash_candidates(self,
                                   trace,
                                   crash_candidates_file,
                                   cache,
                                   crash_validator):
        self.trace = trace
        self.cache = cache
        self.crash_validator = crash_validator

        # get crash candidates
        self.crash_candidates = CrashCandidates()
        self.crash_candidates.init_from_crash_candidates(trace,
                                                         crash_candidates_file)

    # init from a crash target
    def init_for_crash_target(self,
                              trace,
                              crash_target,
                              cache,
                              crash_validator):
        self.trace = trace
        self.cache = cache
        self.crash_validator = crash_validator
        # tell the crash validator to keep pm files
        self.crash_validator.enable_keep_pm_file()
        # crash target: tx_id-fence_id-crash_op_id
        self.crash_target = crash_target

    def run(self):
        # run trace init ops with crashing disabled
        self.accept_trace_init_ops()
        # run trace init ops with crashing enabled
        self.accept_trace_txs()

    # accept all stores in trace init ops
    def accept_trace_init_ops(self):
        getLogger().debug("accepting trace init ops")
        first_tx_range = self.trace.atomic_write_ops_tx_ranges[0]
        ops = self.trace.atomic_write_ops[0:first_tx_range[0]]
        self.accept_trace_tx(ops, None, None)

    # accept all txs
    def accept_trace_txs(self):
        getLogger().debug("accepting trace txs")
        tx_index = 0
        last_index = -1
        for tx_range in self.trace.atomic_write_ops_tx_ranges:
            # accept all ops between TXs without crash
            # TODO can merge with accept_init_ops
            if last_index != -1 and tx_range[0] > last_index:
                getLogger().debug("accepting init ops from %d to %d", \
                                  last_index, tx_range[0]-1)
                init_ops = self.trace.atomic_write_ops[last_index:tx_range[0]]
                self.accept_trace_tx(init_ops, None, None)

            getLogger().debug("accepting trace tx:%d", tx_index)
            # all ops in this tx, not including TX_START and TX_END
            ops = self.trace.atomic_write_ops[tx_range[0]+1:tx_range[1]]
            self.accept_trace_tx(ops, self.try_crash_candidates, tx_index)

            tx_index = tx_index + 1
            last_index = tx_range[1]+1

    # accept one tx
    def accept_trace_tx(self, ops, crash_func, tx_index):
        for op in ops:
            if self.cache.accept(op):
                if crash_func != None:
                    processed_candidates = crash_func(tx_index, op.id, op)
                self.cache.write_back_all_flushing_stores()
                if crash_func != None:
                    self.bring_back_potential_candidates(processed_candidates,\
                                                         tx_index)
        # TODO: missing flushes
        self.cache.write_back_all_stores()

    # get a set of verified crash plans and then crash and validate
    def try_crash_candidates(self, tx_index, fence_index, fence_op):
        # verify all crash candidates before the fence_index ins this tx
        verified_crash_plans, processed_candidates = \
                   self.verify_crash_candidates(tx_index, fence_index, fence_op)
        # getLogger().debug("verified_crash_plans: " + str(verified_crash_plans))

        # validate the test plans
        getLogger().debug("validating at fence:%d begins", fence_index)
        self.crash_validator.validate(tx_index,
                                      fence_index,
                                      verified_crash_plans)
        getLogger().debug("validating at fence:%d ends", fence_index)
        return processed_candidates

    # verify all crash candidates before the fence_index ins this tx
    def verify_crash_candidates(self, tx_index, fence_index, fence_op):
        crash_plans = set()
        processed_candidates = list()

        # get crash candidates in this tx
        crash_candidates = \
                       self.crash_candidates.crash_candidates_per_tx[tx_index]
        # getLogger().debug("crash_candidates: " + str(crash_candidates))
        while(len(crash_candidates) > 0):
            crash_candidate = crash_candidates[0]
            p_store = crash_candidate[0]
            v_store = crash_candidate[1]
            assert(p_store.id != v_store.id)
            assert(p_store.id != fence_index)
            assert(v_store.id != fence_index)
            # when one of the stores is after the fence, we need to return
            if max(p_store.id, v_store.id) > fence_index:
                return crash_plans, processed_candidates

            # pop the valid candidate out and put it into
            processed_candidates.append(crash_candidates.pop(0))

            # if both of them are not fenced (volatile in the cache)
            if not p_store.is_fenced() and not v_store.is_fenced():
                # if the p_store happens first, no matter whether they are in
                # the same cacheline, we are able to only persist p_store and
                # leave v_store volatile
                if p_store.id < v_store.id:
                    crash_plans.add(p_store)
                # if the v_store happens first, only do it when they are in
                # different cachelines
                else:
                    if get_cacheline_address(p_store.address) != \
                            get_cacheline_address(v_store.address):
                        crash_plans.add(p_store)

            # if p_store is already persisted
            if p_store.is_fenced():
                # p_store and v_store cannot be both fenced here
                # because the last fence already processed the candidate
                assert(not v_store.is_fenced())
                # If a crash plan is a fence op, it means persisting nothing
                crash_plans.add(fence_op)

        return crash_plans, processed_candidates

    # bring back potential candidates from processed
    def bring_back_potential_candidates(self, processed_candidates, tx_index):
        crash_candidates = \
                       self.crash_candidates.crash_candidates_per_tx[tx_index]
        # potential candidates should be processed in next fence(s):
        # (1) both of them are still volatile after the fence
        # (2) p_store is persisted but v_store is still volatile after the fence
        for processed_candidate in processed_candidates[::-1]:
            p_store = processed_candidate[0]
            v_store = processed_candidate[1]
            if (not p_store.is_fenced() and not v_store.is_fenced()) or \
                    (p_store.is_fenced() and not v_store.is_fenced()):
                crash_candidates.insert(0, processed_candidate)

    # directly run for the target, so we don't need the belief stuff
    # once we get the target, we just leave
    def run_for_target(self):
        # crash target: tx_id-fence_id-crash_op_id
        target_entries = self.crash_target.split('-')
        target_tx_index = int(target_entries[0])
        target_fence_id = int(target_entries[1])
        target_crash_op_id = int(target_entries[2])
        target_crash_op = self.trace.atomic_write_ops[target_crash_op_id]
        assert(isinstance(target_crash_op, Store) or \
               isinstance(target_crash_op, Fence))
        assert(target_crash_op.id == target_crash_op_id)

        first_tx_start_id = self.trace.atomic_write_ops_tx_ranges[0][0]
        for op in self.trace.atomic_write_ops:
            # if it is fence
            if self.cache.accept(op):
                # if the fence match the target
                if op.id == target_fence_id:
                    # crash and validate then return
                    self.crash_validator.validate(target_tx_index,
                                                  target_fence_id,
                                                  [target_crash_op])
                    return
                # if it is fence then write back all flushing stores
                self.cache.write_back_all_flushing_stores()
            # if it is TXEnd then write back all stores
            if isinstance(op, TXEnd):
                self.cache.write_back_all_stores()
            # if it is the one right before the first TXStart
            # then write back all stores
            if op.id == first_tx_start_id - 1:
                self.cache.write_back_all_stores()
