from mem.witchertrace import WitcherTrace
from misc.witcherexceptions import NotSupportedOperationException
from engines.witcher.binaryfile import BinaryFile
from engines.witcher.pmdktracehandler import PMDKTraceHandler
from engines.witchertx.witchertxresprinter import WitcherTxResPrinter
from engines.witchertx.crashpointgenerator import CrashPointGenerator
from engines.witchertx.witchertxcache import WitcherTxCache
from engines.witchertx.witchertxcrashmanager import WitcherTxCrashManager
from engines.witchertx.witchertxcrashvalidator import WitcherTxCrashValidator
import os

class WitcherTxEngine:
    def __init__(self, args):
        # initialize the trace
        self.init_trace(args.tracefile)
        # initialize the crash points
        self.init_crash_point_generator(args.ppdg)
        # initialize the replay pm file
        self.init_replay_pm_file(args.output_dir,
                                 args.pmdk_mmap_file,
                                 args.pmdk_mmap_size)
        # initialize the cache
        self.init_cache(args.pmdk_mmap_base_addr,
                        args.pmdk_op_tracefile,
                        args.pmdk_val_tracefile)
        # initialize the crash_manager
        self.init_crash_manager(args.validate_exe,
                                args.pmdk_mmap_base_addr,
                                args.pmdk_mmap_size,
                                args.pmdk_create_layout,
                                args.validate_op_file,
                                args.full_oracle_file,
                                args.output_dir)

        # initialize the result printer and print init res
        self.res_printer = WitcherTxResPrinter(args.output_dir)
        self.res_printer.print_trace(self.trace)
        self.res_printer.print_crash_point_list( \
                              self.crash_point_generator.get_crash_point_list())
        self.res_printer.print_double_logging_list( \
                           self.crash_point_generator.get_double_logging_list())

    # run: replay run and crash
    def run(self):
        self.crash_manager.run()
        self.res_printer.print_reported_crash_point_list( \
                             self.crash_manager.get_reported_crash_point_list())

    # initialize the trace
    def init_trace(self, tracefile):
        self.trace = WitcherTrace(tracefile)

    # initialize the crash points
    def init_crash_point_generator(self, ppdg):
        self.crash_point_generator = CrashPointGenerator(self.trace, ppdg)

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
        self.cache = WitcherTxCache(self.binary_file, self.pmdk_trace_handler)

    # initialize the crash manager
    def init_crash_manager(self,
                           validate_exe,
                           mmap_addr,
                           mmap_size,
                           pmdk_pool_layout,
                           op_file,
                           full_oracle_file,
                           output_dir):
        self.crash_validator = WitcherTxCrashValidator(validate_exe,
                                                       self.replay_pm_file,
                                                       mmap_addr,
                                                       mmap_size,
                                                       pmdk_pool_layout,
                                                       op_file,
                                                       full_oracle_file,
                                                       output_dir)
        self.crash_manager = WitcherTxCrashManager(self.trace,
                                                   self.cache,
                                                   self.crash_validator,
                                                   self.crash_point_generator)
