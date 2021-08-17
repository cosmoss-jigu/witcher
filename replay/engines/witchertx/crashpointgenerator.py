from mem.memoryoperations import Store
from mem.memoryoperations import PMDKTXAdd
from mem.memoryoperations import PMDKTXAlloc
from mem.memoryoperations import PMDKCall
from misc.utils import range_cmp, range_contains
from engines.witcher.witcherppdg import WitcherPPDGs
from engines.witcher.witcherbeliefdatabase import WitcherBeliefDatabase
import os.path
import sys

# Crash right before the tx_end to test the undo logging
class CrashPointNoLogging:
    def __init__(self, tx_end_op, store_without_logging_list, witcher_tx_index):
        self.tx_end_op = tx_end_op
        self.store_without_logging_list = store_without_logging_list
        self.witcher_tx_index = witcher_tx_index
        self.id = tx_end_op.id

    def __repr__(self):
        ret = "CrashPointNoLogging:\n" + \
              "\tid:" + str(self.id) + '\n' + \
              "\ttx_end_op:" + str(self.tx_end_op) +  '\n' + \
              "\tstores:"
        for store in self.store_without_logging_list:
            ret = ret + '\n\t\t' + str(store)
        return ret

    def __str__(self):
        ret = "CrashPointNoLogging:\n" + \
              "\tid:" + str(self.id) + '\n' + \
              "\ttx_end_op:" + str(self.tx_end_op) +  '\n' + \
              "\tstores:"
        for store in self.store_without_logging_list:
            ret = ret + '\n\t\t' + str(store)
        return ret

# Crash right after the tx_end
class CrashPointInterTx:
    def __init__(self, tx_end_op, witcher_tx_index):
        self.tx_end_op = tx_end_op
        self.witcher_tx_index = witcher_tx_index
        self.id = tx_end_op.id + 1

    def __repr__(self):
        ret = "CrashPointInterTx:\n" + \
              "\tid:" + str(self.id) + '\n' + \
              "\ttx_end_op:" + str(self.tx_end_op)
        return ret

    def __str__(self):
        ret = "CrashPointInterTx:\n" + \
              "\tid:" + str(self.id) + '\n' + \
              "\ttx_end_op:" + str(self.tx_end_op)
        return ret

# Crash at the store but not making it persisted
class CrashPointNoTx:
    def __init__(self, witcher_tx_index, store_op):
        self.witcher_tx_index = witcher_tx_index
        self.store_op = store_op
        self.id = store_op.id

    def __repr__(self):
        ret = "CrashPointNoTx:\n" + \
              "\tid:" + str(self.id)
        return ret

    def __str__(self):
        ret = "CrashPointNoTx:\n" + \
              "\tid:" + str(self.id)
        return ret

class GeneratorUnit:
    def __init__(self, ops, witcher_tx_index):
        self.ops = ops
        self.witcher_tx_index = witcher_tx_index

        # a stack for pmdk tx begin, since it can be nested
        self.pmdk_tx_stack = 0
        # each element in the list is: [tx_end, stores...] for each pmdk tx
        self.store_list_by_pmdk_tx = []

        self.crash_point_list =[]
        self.double_logging_list =[]

    def get_crash_point_list(self):
        return self.crash_point_list

    def get_double_logging_list(self):
        return self.double_logging_list

    def get_pmdk_tx_count(self):
        return len(self.store_list_by_pmdk_tx)

    def generate_intra_tx(self):
        for op in self.ops:
            if isinstance(op, PMDKCall) and op.func_name == 'pmemobj_tx_begin':
                self.accept_pmdk_tx_begin()
            elif isinstance(op, PMDKCall) and op.func_name == 'pmemobj_tx_end':
                self.accept_pmdk_tx_end(op)
            elif isinstance(op, PMDKTXAdd):
                self.accept_pmdk_tx_add(op)
            elif isinstance(op, PMDKTXAlloc):
                self.accept_pmdk_tx_alloc(op)
            elif isinstance(op, Store):
                self.accept_store(op)

    # only check this when it consists more that one pmdk txs
    # this need belief_database, which requires ppdg
    def generate_inter_tx(self, belief_database):
        assert(self.get_pmdk_tx_count() > 1)
        for i in range(0, self.get_pmdk_tx_count() - 1):
            for j in range(i + 1, self.get_pmdk_tx_count()):
                # i: previous pmdk tx; j: success pmdk tx.
                ret = self.generate_inter_tx_helper( \
                                                  self.store_list_by_pmdk_tx[i],
                                                  self.store_list_by_pmdk_tx[j],
                                                  belief_database)
                # i determines the crash point, if one j succeeds, the skip
                # all the other j for this i
                if ret == True:
                    break

    def accept_pmdk_tx_begin(self):
        assert(self.pmdk_tx_stack >= 0)
        self.pmdk_tx_stack = self.pmdk_tx_stack + 1
        self.rangeable_list = []
        self.store_without_logging_list = []
        self.store_list = []

    def accept_pmdk_tx_end(self, op):
        assert(self.pmdk_tx_stack > 0)
        self.pmdk_tx_stack= self.pmdk_tx_stack - 1
        if len(self.store_without_logging_list) > 0:
            crash_point = CrashPointNoLogging(op, \
                                              self.store_without_logging_list, \
                                              self.witcher_tx_index)
            self.crash_point_list.append(crash_point)
        # update store_list_by_pmdk_tx.append, the very beginning one is the
        # tx_end op, others are stores
        self.store_list_by_pmdk_tx.append([op] + self.store_list)

    def accept_pmdk_tx_add(self, op):
        self.add_to_rangeable_list(op)

    def accept_pmdk_tx_alloc(self, op):
        self.add_to_rangeable_list(op)

    def accept_store(self, op):
        # if the store is not inside a pmdk tx
        if self.pmdk_tx_stack == 0:
            crash_point = CrashPointNoTx(self.witcher_tx_index, op)
            self.crash_point_list.append(crash_point)
            return

        protected = False
        for rangeable in self.rangeable_list:
            if range_contains(rangeable, op):
                protected = True
                break
        # if the store is protected by logging
        if protected == False:
            self.store_without_logging_list.append(op)

        self.store_list.append(op)

    def add_to_rangeable_list(self, op):
        if isinstance(op, PMDKTXAdd):
            for rangeable in self.rangeable_list:
                # if double logging
                if isinstance(rangeable, PMDKTXAdd) and \
                        range_cmp(rangeable, op) == 0:
                    self.double_logging_list.append([rangeable, op])
        self.rangeable_list.append(op)

    def generate_inter_tx_helper(self,
                                 pre_tx_stores,
                                 suc_tx_stores,
                                 belief_db):
        pre_tx_end_op = pre_tx_stores[0]
        for i in range(1, len(pre_tx_stores)):
            pre_st = pre_tx_stores[i]
            for j in range(1, len(suc_tx_stores)):
                suc_st = suc_tx_stores[j]
                # check belief violation here
                if self.violate_atomicity_belief(pre_st, suc_st, belief_db) or \
                        self.violate_ordering_belief(pre_st, suc_st, belief_db):
                    crash_point = CrashPointInterTx(pre_tx_end_op, \
                                                    self.witcher_tx_index)
                    # we need to maintain the order by id here
                    self.add_crash_point_in_order(crash_point)
                    # if it violates belief, we return and skip checking all
                    # others
                    return True
        return False

    def violate_atomicity_belief(self, pre_st, suc_st, belief_database):
        # check whether they are both critical stores
        return belief_database.is_critical_store(pre_st) and \
               belief_database.is_critical_store(suc_st)

    def violate_ordering_belief(self, p_st, v_st, belief_database):
        # Here we want to persist p_st and leave v_st volatile
        # which means we need to find a belief: v_st_obj -hb-> p_st_obj
        return belief_database.violate_ordering_belief(p_st, v_st)

    def add_crash_point_in_order(self, crash_point):
        # TODO a quick and dirty way for now
        self.crash_point_list.append(crash_point)
        self.crash_point_list.sort(key = lambda x: x.id)

class CrashPointGenerator:
    def __init__(self, trace, ppdg):
        self.trace = trace
        self.ppdg = ppdg
        self.belief_database = None

        self.unit_list = []
        self.crash_point_list = []
        self.double_logging_list = []

        self.create_units()
        self.run_units()

    def get_crash_point_list(self):
        return self.crash_point_list

    def get_double_logging_list(self):
        return self.double_logging_list

    def create_units(self):
        witcher_tx_index = 0
        for tx_range in self.trace.atomic_write_ops_tx_ranges:
            ops = self.trace.atomic_write_ops[tx_range[0]+1:tx_range[1]]
            unit = GeneratorUnit(ops, witcher_tx_index)
            self.unit_list.append(unit)
            witcher_tx_index = witcher_tx_index + 1

    def run_units(self):
        for unit in self.unit_list:
            unit.generate_intra_tx()
            if unit.get_pmdk_tx_count() > 1:
                unit.generate_inter_tx(self.get_belief_database())
            self.crash_point_list.extend(unit.get_crash_point_list())
            self.double_logging_list.extend(unit.get_double_logging_list())

    # we only need belief_database for inter_tx checking
    def get_belief_database(self):
        if self.belief_database != None:
            return self.belief_database

        # TODO
        if not os.path.isfile(self.ppdg):
            sys.exit('run make main.ppdg to get ppdg first')

        self.belief_database = WitcherBeliefDatabase(WitcherPPDGs(self.ppdg))
        return self.belief_database
