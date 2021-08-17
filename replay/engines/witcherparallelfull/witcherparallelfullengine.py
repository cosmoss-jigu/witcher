from mem.witchertrace import WitcherTrace
from engines.witcherparallel.witcherparallelresprinter import WitcherParallelResPrinter
from engines.yat.yatengine import YatCache, YatPermutator
from mem.memoryoperations import TXStart, PMDKCall, TXEnd, Store, Flush, Fence
import os
import pickle
from concurrent.futures import ProcessPoolExecutor, wait
import datetime
import csv

PICKLE_ARGS = 'args.pickle'
PICKLE_TRACE = 'trace.pickle'
PICKLE_VALIDATE_RES = 'validate_res.pickle'
PICKLE_RES = 'res.pickle'

BUDGET = 1000

class WitcherParallelFullEngine:
    def __init__(self, args):
        self.overhead_f = open('overhead.txt', 'w')
        # initialize the output dir
        # initialize the witcher parallel path
        # pickle the args
        self.init_misc(args)

        # initialize and pickle the trace
        self.init_trace(args.tracefile)

        # initialize the result printer
        self.res_printer = WitcherParallelResPrinter(self.output)
        # print trace and belief
        self.res_printer.print_trace(self.trace)

        # initialize the pool executor
        self.init_pool_executor()

    def run(self):
        self.cache = YatCache()
        self.permutator = YatPermutator(self.cache)
        self.permutation_count_list = []
        self.permutation_count = 0

        # start from the first tx
        ops = self.trace.atomic_write_ops
        first_tx_start_index = self.trace.atomic_write_ops_tx_ranges[0][0]
        ops = ops[first_tx_start_index:]

        in_TX = False
        for op in ops:
            if isinstance(op, TXStart):
                assert(in_TX == False)
                in_TX = True
                self.permutation_count = 0
                self.cache.flush_all()
            elif isinstance(op, TXEnd):
                assert(in_TX == True)
                in_TX = False
                self.permutation_count_list.append(self.permutation_count)
                self.cache.flush_all()
            elif in_TX and self.cache.accept(op) == True:
                self.permutation_count += self.permutator.run()
                self.cache.flush()

        with open('full.txt', 'w') as f:
            op_count = 0
            total = 0
            for permutation_count in self.permutation_count_list:
                if (permutation_count < 100000):
                    total += permutation_count
                else:
                    f.write(str(op_count) + ':' + str(permutation_count) + '\n')
                op_count += 1
            f.write('Others:' + str(total) + '\n')

        with open('full.csv', 'w', newline='') as file:
            writer = csv.writer(file)
            writer.writerow(['op', 'per_op_count', "total_count"])
            op_count = 0
            total = 0
            for permutation_count in self.permutation_count_list:
                total += permutation_count
                writer.writerow([op_count, permutation_count, total])
                op_count += 1

        # parallel for each TX
        futures = []
        args = self.output + '/' + PICKLE_ARGS

        self.output_tx_dirs = []
        t0 = datetime.datetime.now()
        for tx_id in range(len(self.trace.atomic_write_ops_tx_ranges)):
            # TODO
            #if tx_id != 0:
            #    continue
            #if tx_id != 83:
            #    continue
            #if tx_id != 664:
            #    continue
            total = self.permutation_count_list[tx_id]
            if (total == 0):
                continue
            if tx_id in self.plan:
                plan = self.plan[tx_id]
            else:
                plan = total

            plans = []
            num = int(plan/BUDGET)
            if plan % BUDGET > 0:
                length = int(total/(num+1))
            else:
                length = int(total/num)
            start = 0
            for i in range(num):
                if length < BUDGET:
                    length = BUDGET
                end = start + length - 1
                plans.append([start, end, BUDGET])
                start = end + 1
            if plan % BUDGET > 0:
                plans.append([start, total-1, plan%BUDGET])

            # TODO
            #cnt = 0
            for plan in plans:
                #if cnt != 1 and cnt != 2:
                #    cnt += 1
                #    continue
                cmd = self.exe_path + \
                        '/witcher_parallel_full_unit.py' + \
                        ' -args ' + args + \
                        ' -tx ' + str(tx_id) + \
                        ' -start ' + str(plan[0]) + \
                        ' -end ' + str(plan[1]) + \
                        ' -budget ' + str(plan[2])
                futures.append(self.pool_executor.submit(os.system, cmd))
                self.output_tx_dirs.append(str(tx_id) + '-' + str(plan[0]))
                #cnt += 1

        wait(futures)
        t1 = datetime.datetime.now()
        self.overhead_f.write('validation: ' + str(t1-t0) + '\n')

        # print the result
        self.epilogue()

    # initialize the output dir
    # initialize the witcher parallel path
    # pickle the args
    def init_misc(self, args):
        self.output = args.output_dir
        os.system('mkdir ' + self.output)

        self.exe_path = args.witcher_parallel_path

        pickle.dump(args, open(self.output+'/'+PICKLE_ARGS, 'wb'))

        self.plan = dict()
        plan_list = open(args.plan).read().split("\n")
        plan_list = plan_list[:-1]
        for entry in plan_list:
            strs = entry.split(':')
            self.plan[int(strs[0])] = int(strs[1])

    # initialize and pickle the trace
    def init_trace(self, tracefile):
        output = self.output
        self.trace = WitcherTrace(tracefile)
        pickle.dump(self.trace, open(output+'/'+PICKLE_TRACE, 'wb'))

    # initialize the pool executor
    def init_pool_executor(self):
        self.pool_executor = ProcessPoolExecutor(max_workers=os.cpu_count())

    # print the result
    def epilogue(self):
        tested_crash_plans_per_tx_list = []
        reported_crash_plans_per_tx_list = []
        reported_src_map_per_tx_list = []
        reported_core_dump_map_per_tx_list = []
        reported_priority_per_tx_list = []

        for filename in self.output_tx_dirs:
            path = self.output + '/tx-' + filename + '/' + PICKLE_VALIDATE_RES
            v_res = pickle.load(open(path, 'rb'))

            tested_crash_plans = v_res[0]
            tested_crash_plans_per_tx_list.append(tested_crash_plans)

            reported_crash_plans = v_res[1]
            reported_crash_plans_per_tx_list.append(reported_crash_plans)

            reported_src_map = v_res[2]
            reported_src_map_per_tx_list.append(reported_src_map)

            reported_core_dump_map = v_res[3]
            reported_core_dump_map_per_tx_list.append(reported_core_dump_map)

            reported_priority = v_res[4]
            reported_priority_per_tx_list.append(reported_priority)

        printer = self.res_printer
        printer.print_tested_crash_plans(tested_crash_plans_per_tx_list)
        printer.print_reported_crash_plans(reported_crash_plans_per_tx_list)
        printer.print_reported_src_map(reported_src_map_per_tx_list)
        printer.print_reported_core_dump_map(reported_core_dump_map_per_tx_list)
        printer.print_reported_priority(reported_priority_per_tx_list)
        printer.print_summary()
        printer.print_tested_crash_plan_csv(tested_crash_plans_per_tx_list)

        # pickle the reported_priority for res_analysis
        pickle.dump(reported_priority_per_tx_list, \
                    open(self.output+'/'+PICKLE_RES, 'wb'))
