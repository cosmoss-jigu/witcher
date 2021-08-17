from mem.witchertracemt import WitcherTraceMt
from engines.witcher.witcherppdg import WitcherPPDGs
from engines.witcher.witcherbeliefdatabase import WitcherBeliefDatabase
from engines.witcher.witchercache import WitcherCache
from engines.witcher.binaryfile import BinaryFile
from engines.witcher.pmdktracehandler import PMDKTraceHandler
from engines.witcher.witcherresprinter import WitcherResPrinter
from engines.witchermt.witchercrashmanagermt import WitcherCrashManagerMt
from engines.witchermt.witchercrashvalidatormt import WitcherCrashValidatorMt
import os

class WitcherMtEngine:
    def __init__(self, args):
        # initialize the trace
        self.init_trace(args.tracefile,
                        args.parallel_start_index,
                        args.num_threads)
        # initialize the ppdg and belief, and pickle the belief
        self.init_ppdg_and_belief(args.ppdg)
        # create the dir and the replay pm file
        self.init_replay_pm_file(args.output_dir,
                                 args.pmdk_mmap_file,
                                 args.pmdk_mmap_size)
        # initialize the mmap util, pmdk trace handler and cache
        self.init_cache(args.pmdk_mmap_base_addr,
                        args.pmdk_op_tracefile,
                        args.pmdk_val_tracefile)
        # initialize the crash manager
        self.init_crash_manager(args.validate_exe,
                                args.pmdk_mmap_base_addr,
                                args.pmdk_mmap_size,
                                args.pmdk_create_layout,
                                args.validate_op_file,
                                args.output_dir)
        # initialize the result printer
        self.res_printer = WitcherResPrinter(args.output_dir)
        # print init result
        self.res_printer.print_trace(self.trace)
        self.res_printer.print_belief_database(self.belief_database)
        self.res_printer.print_crash_candidates( \
                                            self.crash_manager.crash_candidates)

    # run: replay run and crash
    def run(self):
        self.crash_manager.run()
        self.res_printer.print_crash_plans(self.crash_validator)
        self.res_printer.print_summary()

    # initialize the trace
    def init_trace(self, tracefile, parallel_start_index, num_threads):
        self.parallel_start_index = int(parallel_start_index)
        self.num_threads = int(num_threads)
        self.trace = WitcherTraceMt(tracefile,
                                    self.parallel_start_index,
                                    self.num_threads)

    # initialize the ppdg and belief, and pickle the belief
    def init_ppdg_and_belief(self, ppdg):
        self.witcher_ppdgs = WitcherPPDGs(ppdg)
        self.belief_database = WitcherBeliefDatabase(self.witcher_ppdgs)

    # create the dir and the replay pm file
    def init_replay_pm_file(self,
                            output_dir,
                            pmdk_mmap_file,
                            pmdk_mmap_size):
        self.replay_pm_file = output_dir + '/' + pmdk_mmap_file

        os.system('mkdir ' + output_dir)

        with open(self.replay_pm_file, "w") as out:
            out.truncate(int(pmdk_mmap_size) * 1024 * 1024)
            out.close()

    # initialize the mmap util, pmdk trace handler and cache
    def init_cache(self, mmap_addr, pmdk_op_tracefile, pmdk_val_tracefile):
        self.binary_file = BinaryFile(self.replay_pm_file, mmap_addr)
        self.pmdk_trace_handler = PMDKTraceHandler(self.binary_file,
                                                   pmdk_op_tracefile,
                                                   pmdk_val_tracefile)
        self.cache = WitcherCache(self.binary_file, self.pmdk_trace_handler)

    # initialize the crash manager
    def init_crash_manager(self,
                           validate_exe,
                           mmap_addr,
                           mmap_size,
                           pmdk_pool_layout,
                           op_file,
                           output_dir):
        self.crash_validator = WitcherCrashValidatorMt(self.cache,
                                                       validate_exe,
                                                       self.replay_pm_file,
                                                       mmap_addr,
                                                       mmap_size,
                                                       pmdk_pool_layout,
                                                       op_file,
                                                       self.parallel_start_index,
                                                       self.num_threads,
                                                       output_dir)
        self.crash_manager = WitcherCrashManagerMt(self.trace,
                                                   self.belief_database,
                                                   self.cache,
                                                   self.crash_validator)
