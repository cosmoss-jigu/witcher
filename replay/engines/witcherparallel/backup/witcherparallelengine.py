from mem.witchertrace import WitcherTrace
from engines.witcher.witcherppdg import WitcherPPDGs
from engines.witcher.witcherbeliefdatabase import WitcherBeliefDatabase
from engines.witcherparallel.witcherparallelresprinter import WitcherParallelResPrinter
from engines.witcherparallel.witcherparallelserver import WitcherParallelServer
import os
import pickle

PICKLE_ARGS = 'args.pickle'
PICKLE_TRACE = 'trace.pickle'
PICKLE_BELIEF= 'belief.pickle'

class WitcherParallelEngine:
    def __init__(self, args):
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

        # initialize the server
        self.init_server()

    def run(self):
        # get crash candidates in parallel
        # each job will call validate internal
        args = self.output + '/' + PICKLE_ARGS
        trace = self.output + '/' + PICKLE_TRACE
        belief = self.output + '/' + PICKLE_BELIEF
        for tx_id in range(len(self.trace.atomic_write_ops_tx_ranges)):
            cmd = self.exe_path + \
                    '/witcher_parallel_crashcandidates.py' + \
                    ' -args ' + args + \
                    ' -t ' + trace + \
                    ' -b ' + belief + \
                    ' -tx ' + str(tx_id)
            self.server.submit_to_executor(cmd)

    # initialize the output dir
    # initialize the witcher parallel path
    # pickle the args
    def init_misc(self, args):
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
        self.belief_database = WitcherBeliefDatabase(self.witcher_ppdgs)
        pickle.dump(self.belief_database, open(output+'/'+PICKLE_BELIEF, 'wb'))

    # initialize the server
    def init_server(self):
        num_of_txs = len(self.trace.atomic_write_ops_tx_ranges)
        self.server = WitcherParallelServer(self.res_printer, num_of_txs)
