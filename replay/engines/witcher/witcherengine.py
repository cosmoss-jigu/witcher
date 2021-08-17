from engines.replayengine import ReplayEngineBase
from engines.witcher.witcherppdg import WitcherPPDGs
from engines.witcher.witcherbeliefdatabase import WitcherBeliefDatabase
from engines.witcher.witchercache import WitcherCache
from engines.witcher.binaryfile import BinaryFile
from engines.witcher.witchercrashmanager import WitcherCrashManager
from engines.witcher.witchercrashvalidator import WitcherCrashValidator
from engines.witcher.pmdktracehandler import PMDKTraceHandler
from engines.witcher.witcherresprinter import WitcherResPrinter
from logging import getLogger
import os

class WitcherEngine(ReplayEngineBase):
    def __init__(self):
        pass

    # set the trace
    def set_trace(self, trace):
        self.trace = trace

    # initialize the witcher
    def init_witcher(self, args):
        self.init_ppdg_belief_test(args.ppdg)
        self.init_replay_pm_file(args.output_dir,
                                 args.pmdk_mmap_file,
                                 args.pmdk_mmap_size)
        self.init_cache(args.pmdk_mmap_base_addr,
                        args.pmdk_op_tracefile,
                        args.pmdk_val_tracefile)
        self.init_crash_manager(args.validate_exe,
                                args.pmdk_mmap_base_addr,
                                args.pmdk_mmap_size,
                                args.pmdk_create_layout,
                                args.validate_op_file,
                                args.full_oracle_file,
                                args.output_dir)

        # initialize the result printer
        self.res_printer = WitcherResPrinter(args.output_dir)
        # print init result
        self.res_printer.print_trace(self.trace)
        self.res_printer.print_belief_database(self.belief_database)
        self.res_printer.print_crash_candidates( \
                                            self.crash_manager.crash_candidates)

    # initialize the witcher from given crash_candidates
    def init_witcher_from_crash_candidates(self, args):
        self.init_replay_pm_file(args.output_dir,
                                 args.pmdk_mmap_file,
                                 args.pmdk_mmap_size)
        self.init_cache(args.pmdk_mmap_base_addr,
                        args.pmdk_op_tracefile,
                        args.pmdk_val_tracefile)
        self.init_crash_manager_from_crash_candidates( \
                                args.validate_exe,
                                args.pmdk_mmap_base_addr,
                                args.pmdk_mmap_size,
                                args.pmdk_create_layout,
                                args.validate_op_file,
                                args.full_oracle_file,
                                args.output_dir,
                                args.crash_candidates)

        # initialize the result printer
        self.res_printer = WitcherResPrinter(args.output_dir)
        # print init result
        self.res_printer.print_trace(self.trace)
        self.res_printer.print_crash_candidates( \
                                            self.crash_manager.crash_candidates)

    # initialize the witcher from given crash target
    def init_witcher_for_crash_target(self, args):
        self.init_replay_pm_file(args.output_dir,
                                 args.pmdk_mmap_file,
                                 args.pmdk_mmap_size)
        self.init_cache(args.pmdk_mmap_base_addr,
                        args.pmdk_op_tracefile,
                        args.pmdk_val_tracefile)
        self.init_crash_manager_for_crash_target( \
                                args.validate_exe,
                                args.pmdk_mmap_base_addr,
                                args.pmdk_mmap_size,
                                args.pmdk_create_layout,
                                args.validate_op_file,
                                args.full_oracle_file,
                                args.output_dir,
                                args.crash_target)

        # initialize the result printer
        self.res_printer = WitcherResPrinter(args.output_dir)
        # print init result
        self.res_printer.print_trace(self.trace)

    # initialize the ppdg
    def init_ppdg_belief_test(self, ppdg):
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
                           full_oracle_file,
                           output_dir):
        self.crash_validator = WitcherCrashValidator(self.cache,
                                                     validate_exe,
                                                     self.replay_pm_file,
                                                     mmap_addr,
                                                     mmap_size,
                                                     pmdk_pool_layout,
                                                     op_file,
                                                     full_oracle_file,
                                                     output_dir)
        self.crash_manager = WitcherCrashManager()
        self.crash_manager.init_from_belief_database(self.trace,
                                                     self.belief_database,
                                                     self.cache,
                                                     self.crash_validator)

    # initialize the crash manager from crash candidates
    def init_crash_manager_from_crash_candidates(self,
                                                 validate_exe,
                                                 mmap_addr,
                                                 mmap_size,
                                                 pmdk_pool_layout,
                                                 op_file,
                                                 full_oracle_file,
                                                 output_dir,
                                                 crash_candidates_file):
        self.crash_validator = WitcherCrashValidator(self.cache,
                                                     validate_exe,
                                                     self.replay_pm_file,
                                                     mmap_addr,
                                                     mmap_size,
                                                     pmdk_pool_layout,
                                                     op_file,
                                                     full_oracle_file,
                                                     output_dir)
        self.crash_manager = WitcherCrashManager()
        self.crash_manager.init_from_crash_candidates(self.trace,
                                                      crash_candidates_file,
                                                      self.cache,
                                                      self.crash_validator)

    # initialize the crash manager from crash target
    def init_crash_manager_for_crash_target(self,
                                            validate_exe,
                                            mmap_addr,
                                            mmap_size,
                                            pmdk_pool_layout,
                                            op_file,
                                            full_oracle_file,
                                            output_dir,
                                            crash_target):
        self.crash_validator = WitcherCrashValidator(self.cache,
                                                     validate_exe,
                                                     self.replay_pm_file,
                                                     mmap_addr,
                                                     mmap_size,
                                                     pmdk_pool_layout,
                                                     op_file,
                                                     full_oracle_file,
                                                     output_dir)
        self.crash_manager = WitcherCrashManager()
        self.crash_manager.init_for_crash_target(self.trace,
                                                 crash_target,
                                                 self.cache,
                                                 self.crash_validator)

    # run: replay run and crash
    def run(self):
        self.crash_manager.run()
        self.res_printer.print_crash_plans(self.crash_validator)
        self.res_printer.print_summary()

    # run: replay run and crash for crash target
    def run_for_target(self):
        self.crash_manager.run_for_target()
