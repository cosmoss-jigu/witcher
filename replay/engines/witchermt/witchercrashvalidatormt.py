from engines.witcher.witchercrashvalidator import WitcherCrashValidator
from logging import getLogger
import os
import subprocess
from subprocess import Popen, PIPE, TimeoutExpired
import itertools

class WitcherCrashValidatorMt(WitcherCrashValidator):
    def __init__(self, cache, validate_exe, replay_pm_file, mmap_addr,
                 mmap_size, pmdk_pool_layout, op_file, parallel_start_index,
                 num_threads, output_dir):
        self.cache = cache

        self.validate_exe = validate_exe
        self.replay_pm_file = replay_pm_file
        self.mmap_addr = mmap_addr
        self.mmap_size = mmap_size
        self.pmdk_pool_layout = pmdk_pool_layout
        self.op_file = op_file
        self.parallel_start_index = parallel_start_index
        self.num_threads = num_threads

        self.output_dir = output_dir

        # get ops for constructing oracle op_files
        ops = open(self.op_file).read().split("\n")
        self.ops = ops[:-1]

        # oracles
        # key is the index, value is the start compare index
        self.oracles = dict()

        self.tested_crash_plans  = []
        self.reported_crash_plans = []
        # key is src_info, val is output file
        self.reported_src_map = dict()
        # key is core_dump, val is output file
        self.reported_core_dump_map = dict()
        # map<src_info, map<core_dump, list<output_file>>>
        self.reported_priority = dict()

        # a flag to decide whether we need to keep generated pm files
        self.keep_pm_file = False

    def check_consistency(self, crash_state_file, tx_index, crash_src_info):
        # execute the suffix
        crash_state_output, returncode, crash_core_dump, timeout = \
                       self.execute_from_crash_state(crash_state_file)

        if timeout == True:
            self.update_result(crash_state_output,
                               crash_src_info,
                               crash_core_dump,
                               True,
                               'timeout')
            self.cleanup_pm_file(crash_state_file)
            return

        if returncode != 0:
            # update result
            self.update_result(crash_state_output,
                               crash_src_info,
                               crash_core_dump,
                               True,
                               None)
            # remove the pm file for saving disk space
            self.cleanup_pm_file(crash_state_file)
            return

        # get oracles
        self.get_oracles()

        # compare the output with oracles
        inconsistent, mismatch_res = self.compare_to_oracles(crash_state_output)

        # update result
        self.update_result(crash_state_output,
                           crash_src_info,
                           crash_core_dump,
                           inconsistent,
                           mismatch_res)
        # remove the pm file for saving disk space
        self.cleanup_pm_file(crash_state_file)

    def execute_from_crash_state(self, crash_state_file):
        # construct op_file
        op_file = self.output_dir + '/op_file_crash.txt'
        with open(op_file, 'w') as f:
            for op in self.ops[self.parallel_start_index+self.num_threads:]:
                f.write('%s\n' % op)

        mode = 1
        crash_state_output = crash_state_file + '-output'
        cmd = ['./' + self.validate_exe,
               crash_state_file,
               self.mmap_size,
               self.pmdk_pool_layout,
               op_file,
               str(self.parallel_start_index),
               str(mode),
               str(self.num_threads),
               crash_state_output]

        getLogger().debug('validate cmd: ' + ' '.join(cmd))
        proc = Popen(cmd, stdout=PIPE, stderr=PIPE)
        # set the timeout for validation
        # TODO 10s for now
        timeout = False
        try:
            proc.communicate(timeout=10)
        except TimeoutExpired:
            proc.kill()
            proc.communicate()
            timeout = True

        crash_core_dump = None
        # get the core dump if the return code is not 0
        if timeout == False and proc.returncode != 0:
            crash_core_dump = self.get_crash_core_dump(proc.pid)
        return crash_state_output, proc.returncode, crash_core_dump, timeout

    # get oracles
    def get_oracles(self):
        if (len(self.oracles) > 0):
            return

        # construct op_file
        ops_prefix = self.ops[:self.parallel_start_index]
        ops_crash = self.ops[self.parallel_start_index:\
                             self.parallel_start_index+self.num_threads]
        ops_suffix = self.ops[self.parallel_start_index+self.num_threads:]
        oracle_count = 0
        for length in range(0, self.num_threads + 1):
            for ops_iter in itertools.permutations(ops_crash, length):
                oracle_ops_crash = list(ops_iter)
                oracle_op_file = self.output_dir + '/oracle_op_' + \
                                 str(oracle_count)
                oracle_ops = ops_prefix + oracle_ops_crash + ops_suffix
                with open(oracle_op_file, 'w') as f:
                    for op in oracle_ops:
                        f.write('%s\n' % op)
                self.oracles[oracle_count] = self.parallel_start_index + length
                oracle_count += 1

        # construct oracle output
        mode = 1
        for oracle_count in self.oracles:
            oracle_op_file = self.output_dir + '/oracle_op_' + \
                             str(oracle_count)
            oracle_output = self.output_dir + '/oracle_output_' + \
                            str(oracle_count)
            oracle_pm = oracle_output + '-pm'
            cmd = ['./' + self.validate_exe,
                   oracle_pm,
                   self.mmap_size,
                   self.pmdk_pool_layout,
                   oracle_op_file,
                   str(self.parallel_start_index),
                   str(mode),
                   str(self.num_threads),
                   oracle_output]
            subprocess.call(cmd, stdout=PIPE, stderr=PIPE)

            # remove the pm file for saving disk space
            self.cleanup_pm_file(oracle_pm)

    def compare_to_oracles(self, crash_state_output):
        match_list = []
        mismatch_msg_list = []

        for oracle_count in self.oracles:
            oracle_output = self.output_dir + '/oracle_output_' + \
                            str(oracle_count)
            start_cmp_index = self.oracles[oracle_count]
            match, tx_mismatch = self.compare_to_oracle_from_index(\
                             crash_state_output, oracle_output, start_cmp_index)
            match_list.append(match)
            mismatch_msg_list.append(tx_mismatch)

        # if one of it match, then we will not mark it as inconsistency
        inconsistent = True
        for match in match_list:
            if match == True:
                inconsistent = False
                break;

        return inconsistent, ' '.join(mismatch_msg_list)
