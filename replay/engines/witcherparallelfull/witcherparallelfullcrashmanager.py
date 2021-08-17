import os
import pickle
import random
from mem.memoryoperations import TXEnd
from engines.witcher.binaryfile import BinaryFile
from engines.witcher.witchercache import WitcherCache
from engines.witcherparallelfull.witcherfullcrashvalidator import WitcherFullCrashValidator
from engines.witcherparallel.witcherparallelengine import PICKLE_TRACE
from engines.witcherparallel.witcherparallelengine import PICKLE_BELIEF
from engines.witcherparallel.witcherparallelengine import PICKLE_VALIDATE_RES


class WitcherParallelFullCrashManager:
    def __init__(self, args, tx_id, start, end, budget):
        # initialize self fields for components
        self.init_misc_0(args, tx_id)
        # plans
        self.init_plans(start, end, budget)
        # initialize trace
        self.init_trace()
        # initialize self fields for components
        self.init_misc_1(args, tx_id)
        # initialize cache
        self.init_cache()
        # initialize crash validate
        self.init_crash_validator()

    def run(self):
        # replay from beginning to target store without crash
        self.run_before_target()
        # replay from target store
        # and try to find right places to crash and validate
        self.run_try_traget()
        ## send results to the server
        self.epilogue()

    # initialize self fields for components
    def init_misc_0(self, args, tx_id):
        self.tx_id = tx_id
        self.output_partent = args.output_dir

    # plans
    def init_plans(self, start, end, budget):
        self.start = start
        self.end = end
        self.budget = budget

        if budget == end - start + 1:
            self.crash_targets = list(range(start, end + 1))
        else:
            self.crash_targets = []
            for i in range(budget):
                rand = random.randint(start, end)
                while rand in self.crash_targets:
                    rand = random.randint(start, end)
                self.crash_targets.append(rand)
            self.crash_targets.sort()

    # initialize self fields for components
    def init_misc_1(self, args, tx_id):
        # initialize output
        self.output = args.output_dir + '/tx-' + str(tx_id) + '-' + str(self.start)
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

    # initialize cache
    def init_cache(self):
        self.binary_file = BinaryFile(self.replay_pm_file, \
                                      self.pmdk_mmap_base_addr)
        self.cache = WitcherCache(self.binary_file)

    # initialize crash validate
    def init_crash_validator(self):
        self.crash_validator = WitcherFullCrashValidator(self.cache,
                                                     self.validate_exe,
                                                     self.replay_pm_file,
                                                     self.pmdk_mmap_base_addr,
                                                     self.pmdk_mmap_size,
                                                     self.pmdk_create_layout,
                                                     self.validate_op_file,
                                                     self.full_oracle_file,
                                                     self.output,
                                                     #self.server_name,
                                                     'na',
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

    def permutation_count(self):
        # Permute all stores in the cache
        permutation_count = 1
        for cacheline in self.cache.get_cachelines():
            num_of_stores = len(cacheline.stores_list)
            permutation_count *= num_of_stores + 1
        permutation_count -= 1
        return permutation_count

    def get_stores_from_target(self, target):
        stores = []
        target += 1
        digits = []
        cachelines = []
        for cacheline in self.cache.get_cachelines():
            num_of_stores = len(cacheline.stores_list)
            digits.append(num_of_stores + 1)
            cachelines.append(cacheline.stores_list)

        last = 1
        for i in reversed(range(0, len(digits))):
            curr_digit = digits[i]
            digits[i] = last
            last *= curr_digit

        count = target
        for i in range(0, len(digits)):
            curr_digit = int(count/digits[i])
            if curr_digit > 0:
                stores.append(cachelines[i][curr_digit-1])
            count %= digits[i]

        return stores

    # replay from target store
    # and try to find right places to crash and validate
    def run_try_traget(self):
        curr_plan_index = 0
        permutation_count = 0

        target_tx_range = self.trace.atomic_write_ops_tx_ranges[self.tx_id]
        ops = self.trace.atomic_write_ops[target_tx_range[0]:target_tx_range[1]]

        for op in ops:
            if self.cache.accept(op):
                last_permutation_count = permutation_count
                permutation_count += self.permutation_count()
                targets = []
                while curr_plan_index < len(self.crash_targets) and \
                        permutation_count > self.crash_targets[curr_plan_index]:
                    targets.append(self.crash_targets[curr_plan_index])
                    curr_plan_index += 1
                crash_plans = []
                for target in targets:
                    stores = self.get_stores_from_target(target-last_permutation_count)
                    crash_plans.append([target, stores])
                self.crash_validator.validate(self.tx_id, op.id, crash_plans)

                # if it is fence then write back all flushing stores
                self.cache.write_back_all_flushing_stores()

                if curr_plan_index == len(self.crash_targets):
                    break
        # TODO: missing flushes
        # write back all stores at the end of TX
        # self.cache.write_back_all_stores()

    # pickle the result
    def epilogue(self):
        v = self.crash_validator
        res = [v.tested_crash_plans, v.reported_crash_plans, \
               v.reported_src_map, v.reported_core_dump_map, \
               v.reported_priority]
        pickle.dump(res, open(self.output+'/'+PICKLE_VALIDATE_RES, 'wb'))
        cmd = 'rm ' + self.output + '/pm.img* ' + self.output + '/oracle*'
        os.system(cmd)
