import os
from subprocess import Popen, PIPE
import subprocess
from engines.witchertx.crashpointgenerator import CrashPointNoLogging
from engines.witchertx.crashpointgenerator import CrashPointInterTx
from engines.witchertx.crashpointgenerator import CrashPointNoTx

class WitcherTxCrashValidator:
    def __init__(self, validate_exe, replay_pm_file, mmap_addr,
                 mmap_size, pmdk_pool_layout, op_file, full_oracle_file,
                 output_dir):
        self.validate_exe = validate_exe
        self.replay_pm_file = replay_pm_file
        self.mmap_addr = mmap_addr
        self.mmap_size = mmap_size
        self.pmdk_pool_layout = pmdk_pool_layout
        self.op_file = op_file
        self.full_oracle_file = full_oracle_file
        self.output_dir = output_dir

        # a dict of oracle paths
        # 'full': 'oracle-full'
        # NUM : '/oracle-skip-tx-$(NUM)'
        self.oracles = dict()

        # a flag to decide whether we need to keep generated pm files
        self.keep_pm_file = False

        # result
        self.reported_crash_point_list = []

    # enable keeping generated pm files
    def enable_keep_pm_file(self):
        self.keep_pm_file = True

    # get the result
    def get_reported_crash_point_list(self):
        return self.reported_crash_point_list

    # validate the crash_point
    def validate(self, crash_point):
        # construct the crash pm first
        crash_state_file = self.construct_crash_state(crash_point)
        # then check its consistency
        self.check_consistency(crash_state_file, crash_point)

    # construct the crash pm by copying current pm
    def construct_crash_state(self, crash_point):
        # file path: replay_pm_file-tx_index-store_id
        crash_state_file = self.replay_pm_file + '-' + \
                           str(crash_point.witcher_tx_index) + '-' + \
                           str(crash_point.id)

        # TODO: should be optimized instead of using cp
        # copy the current replay_pm_file
        cmd = ['cp',
               self.replay_pm_file,
               crash_state_file]
        os.system(' '.join(cmd))

        # here we make a copy of crash pm for debugging
        # since suffix may modify it
        if self.keep_pm_file == True:
            cmd = ['cp',
                   crash_state_file,
                   crash_state_file+'-crash']
            os.system(' '.join(cmd))

        return crash_state_file

    # check the output
    def check_consistency(self, crash_state_file, crash_point):
        # execute the suffix
        crash_state_output, returncode = \
                     self.execute_from_crash_state(crash_state_file, \
                                                   crash_point.witcher_tx_index)

        # if it fails, we just report it
        if returncode != 0:
            self.reported_crash_point_list.append(crash_point)
            # remove the pm file for saving disk space
            self.cleanup_pm_file(crash_state_file)
            return

        # oracle comparison
        if isinstance(crash_point, CrashPointNoLogging):
            # undo logging crash, only 1 oracle
            oracle_skip = self.get_oracle_skip(crash_point.witcher_tx_index)
            match = self.compare_to_oracle(crash_state_output,
                                           oracle_skip,
                                           crash_point.witcher_tx_index)
            if match == False:
                self.reported_crash_point_list.append(crash_point)
        elif isinstance(crash_point, CrashPointInterTx) or \
                isinstance(crash_point, CrashPointNoTx):
            # inter tx crash needs to check 2 oracles
            oracle_full = self.get_oracle_full()
            match_full = self.compare_to_oracle(crash_state_output,
                                                oracle_full,
                                                crash_point.witcher_tx_index+1)

            oracle_skip = self.get_oracle_skip(crash_point.witcher_tx_index)
            match_skip = self.compare_to_oracle(crash_state_output,
                                                oracle_skip,
                                                crash_point.witcher_tx_index)
            if match_full == False and match_skip == False:
                self.reported_crash_point_list.append(crash_point)
        else:
            # never come to here
            assert(False)

        # remove the pm file for saving disk space
        self.cleanup_pm_file(crash_state_file)

    # remove the pm file for saving disk space
    def cleanup_pm_file(self, crash_state_file):
        if self.keep_pm_file == True:
            return

        cmd = ['rm',
               '-f',
               crash_state_file]
        os.system(' '.join(cmd))

    # execute the suffix
    def execute_from_crash_state(self, crash_state_file, wticher_tx_index):
        start_witcher_tx_index = wticher_tx_index + 1
        skip_witcher_tx_index = -1
        crash_state_output = crash_state_file + '-output'
        cmd = ['./' + self.validate_exe,
               crash_state_file,
               self.mmap_size,
               self.pmdk_pool_layout,
               self.op_file,
               str(start_witcher_tx_index),
               str(skip_witcher_tx_index),
               crash_state_output]

        proc = Popen(cmd, stdout=PIPE, stderr=PIPE)
        proc.communicate()
        return crash_state_output, proc.returncode

    # get oracle full
    def get_oracle_full(self):
        # get oracle-full: cp it from the tracing output and put into dict
        if 'full' not in self.oracles:
            oracle_output_path = self.output_dir + '/oracle-full'
            cmd = ['cp',
                   self.full_oracle_file,
                   oracle_output_path]
            os.system(' '.join(cmd))
            self.oracles['full'] = oracle_output_path
        oracle_full = self.oracles['full']
        return oracle_full

    # get oracle skip
    def get_oracle_skip(self, witcher_tx_index):
        # get oracle-skip: run it and put into the dict
        if witcher_tx_index not in self.oracles:
            oracle_output_path = self.output_dir + \
                                 '/oracle-skip-tx-' + \
                                 str(witcher_tx_index)
            oracle_pm_path = oracle_output_path + '-pm'
            start_witcher_tx_index = 0
            skip_witcher_tx_index = witcher_tx_index
            cmd = ['./' + self.validate_exe,
                   oracle_pm_path,
                   self.mmap_size,
                   self.pmdk_pool_layout,
                   self.op_file,
                   str(start_witcher_tx_index),
                   str(skip_witcher_tx_index),
                   oracle_output_path]
            subprocess.call(cmd, stdout=PIPE, stderr=PIPE)
            self.oracles[witcher_tx_index] = oracle_output_path

            # remove the pm file for saving disk space
            self.cleanup_pm_file(oracle_pm_path)
        oracle_skip = self.oracles[witcher_tx_index]

        return oracle_skip

    # compare crash_state_output to oracle from oracle_index
    def compare_to_oracle(self,
                          crash_state_output,
                          oracle,
                          oracle_index):
        if not os.path.isfile(crash_state_output):
            return False
        with open(crash_state_output) as f:
            crash_state_output_list= f.read().splitlines()

        if not os.path.isfile(oracle):
            return False
        with open(oracle) as f:
            oracle_list = f.read().splitlines()
        oracle_list = oracle_list[oracle_index:]

        # the length may be different because execution may crash due to
        # inconsistency
        if len(crash_state_output_list) != len(oracle_list):
            return False

        for i in range(len(crash_state_output_list)):
            if crash_state_output_list[i] != oracle_list[i]:
                return False

        return True
