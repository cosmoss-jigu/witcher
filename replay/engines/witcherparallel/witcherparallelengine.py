from mem.witchertrace import WitcherTrace
from engines.witcher.witcherppdg import WitcherPPDGs
from engines.witcher.witcherbeliefdatabase import WitcherBeliefDatabase
from engines.witcherparallel.witcherparallelresprinter import WitcherParallelResPrinter
import os
import pickle
from concurrent.futures import ProcessPoolExecutor, wait
from engines.witcherutils.WitcherParallelUtils import getThreadPoolStatus,threadPoolDispatchTask,threadPoolWait
import datetime

PICKLE_ARGS = 'args.pickle'
PICKLE_TRACE = 'trace.pickle'
PICKLE_BELIEF= 'belief.pickle'
PICKLE_VALIDATE_RES = 'validate_res.pickle'
PICKLE_RES = 'res.pickle'

class WitcherParallelEngine:
    def __init__(self, args):
        self.overhead_f = open('overhead.txt', 'w')
        # initialize the output dir
        # initialize the witcher parallel path
        # pickle the args
        self.init_misc(args)

        # initialize and pickle the trace
        self.init_trace(args.tracefile)
        # initialize the ppdg and belief, and pickle the belief
        self.init_ppdg_and_belief(args.ppdg)


        # initialize the result printer
        self.res_printer = WitcherParallelResPrinter(self.output)
        # print trace and belief
        self.res_printer.print_trace(self.trace)
        self.res_printer.print_belief_database(self.belief_database)

        # initialize the pool executor
        self.init_pool_executor()

    def run(self):
        # parallel for each TX
        futures = []
        args = self.output + '/' + PICKLE_ARGS

        t0 = datetime.datetime.now()
        for tx_id in range(len(self.trace.atomic_write_ops_tx_ranges)):
            cmd = self.exe_path + \
                    '/witcher_parallel_unit.py' + \
                    ' -args ' + args + \
                    ' -tx ' + str(tx_id)
            if self.useTPL:
                threadPoolDispatchTask(cmd)
            else:
                futures.append(self.pool_executor.submit(os.system, cmd))
        if self.useTPL:
            threadPoolWait()
        else:
            wait(futures)
        t1 = datetime.datetime.now()
        self.overhead_f.write('validation: ' + str(t1-t0) + '\n')

        # print the result
        self.epilogue()

    # initialize the output dir
    # initialize the witcher parallel path
    # pickle the args
    def init_misc(self, args):
        self.useTPL = args.useThreadPool
        self.output = args.output_dir
        os.system('mkdir ' + self.output)

        self.exe_path = args.witcher_parallel_path

        pickle.dump(args, open(self.output+'/'+PICKLE_ARGS, 'wb'))

    # initialize and pickle the trace
    def init_trace(self, tracefile):
        output = self.output
        self.trace = WitcherTrace(tracefile)
        pickle.dump(self.trace, open(output+'/'+PICKLE_TRACE, 'wb'))

    # initialize the ppdg and belief, and pickle the belief
    def init_ppdg_and_belief(self, ppdg):
        output = self.output
        self.witcher_ppdgs = WitcherPPDGs(ppdg)

        t0 = datetime.datetime.now()
        self.belief_database = WitcherBeliefDatabase(self.witcher_ppdgs)
        t1 = datetime.datetime.now()
        self.overhead_f.write('belief_database: ' + str(t1-t0) + '\n')

        pickle.dump(self.belief_database, open(output+'/'+PICKLE_BELIEF, 'wb'))

    # initialize the pool executor
    def init_pool_executor(self):
        self.pool_executor = ProcessPoolExecutor(max_workers=os.cpu_count()-1)
        #self.pool_executor = ProcessPoolExecutor(max_workers=32)

    # print the result
    def epilogue(self):
        crash_candidates_per_tx_list = []
        tested_crash_plans_per_tx_list = []
        reported_crash_plans_per_tx_list = []
        reported_src_map_per_tx_list = []
        reported_core_dump_map_per_tx_list = []
        reported_priority_per_tx_list = []

        for tx_id in range(len(self.trace.atomic_write_ops_tx_ranges)):
            path = self.output + '/tx-' + str(tx_id) + '/' + PICKLE_VALIDATE_RES
            if not os.path.isfile(path):
                crash_candidates_per_tx_list.append([])
                tested_crash_plans_per_tx_list.append([])
                continue
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

            crash_candidates = v_res[5]
            crash_candidates_per_tx_list.append(crash_candidates)

        printer = self.res_printer
        printer.print_crash_candidates(crash_candidates_per_tx_list)
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
