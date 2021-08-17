from engines.witcher.binaryfile import BinaryFile
from mem.memoryoperations import Store, Fence
from logging import getLogger
import os
import subprocess
from subprocess import Popen, PIPE, TimeoutExpired
from engines.witcherparallel.servermemcached import run_memcached
from engines.witcherparallel.serverredis import run_redis

class WitcherCrashValidator:
    def __init__(self, cache, validate_exe, replay_pm_file, mmap_addr,
                 mmap_size, pmdk_pool_layout, op_file, full_oracle_file,
                 output_dir, server_name, tx_id):
        self.cache = cache

        self.validate_exe = validate_exe
        self.replay_pm_file = replay_pm_file
        self.mmap_addr = mmap_addr
        self.mmap_size = mmap_size
        self.pmdk_pool_layout = pmdk_pool_layout
        self.op_file = op_file
        self.full_oracle_file = full_oracle_file

        self.output_dir = output_dir

        self.server_name = server_name
        self.tx_id = tx_id

        # a dict of oracle paths
        # 'full': 'oracle-full'
        # NUM : '/oracle-skip-tx-$(NUM)'
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

    # enable keeping generated pm files
    def enable_keep_pm_file(self):
        self.keep_pm_file = True

    # validate at tx_index fence_index wiht crash_plans
    def validate(self, tx_index, fence_index, crash_plans):
        # validate crash_plan one by one
        for crash_plan in crash_plans:
            # construct the crash pm first
            crash_state_file = self.construct_crash_state(tx_index,
                                                          fence_index,
                                                          crash_plan)
            # then check its consistency
            self.check_consistency(crash_state_file, tx_index, crash_plan.src_info)

    # construct the crash pm by persisting the crash plan on current pm
    def construct_crash_state(self, tx_index, fence_index, crash_plan):
        # get the current pm first
        crash_state_file = self.generate_current_state(tx_index,
                                                       fence_index,
                                                       crash_plan)
        # then persist the crash plan on it
        self.persist_crash_plan(crash_state_file, crash_plan)

        # here we make a copy of crash pm for debugging
        # since suffix may modify it
        if self.keep_pm_file == True:
            cmd = ['cp',
                   crash_state_file,
                   crash_state_file+'-crash']
            os.system(' '.join(cmd))

        return crash_state_file

    # copy the current pm into the output dir
    def generate_current_state(self, tx_index, fence_index, crash_plan):
        # flush the pm.img before copy
        self.cache.binary_file.flush()
        # file path: replay_pm_file-tx_index-store_id
        crash_state_file = self.replay_pm_file + '-' + \
                           str(tx_index) + '-' + \
                           str(fence_index) + '-' + \
                           str(crash_plan.id)

        # TODO: should be optimized instead of using cp
        # copy the current replay_pm_file
        cmd = ['cp',
               self.replay_pm_file,
               crash_state_file]
        os.system(' '.join(cmd))

        return crash_state_file

    # persist the crash plan on the current pm
    def persist_crash_plan(self, crash_state_file, crash_plan):
        # if a crash_plan is a Fence op, it means persisting nothing
        # TODO potential duplicate here
        if isinstance(crash_plan, Fence):
            return
        assert(isinstance(crash_plan, Store))

        # init the binary file of the current pm
        binary_file = BinaryFile(crash_state_file, self.mmap_addr)
        # get all the stores should be persisted
        # (1) crash_plan itself
        # (2) all stores in the same cacheline happens before it
        stores_to_be_persisted = \
                               self.cache.get_stores_from_crash_plan(crash_plan)
        # persist them one by one
        for store_to_be_persisted in stores_to_be_persisted:
            binary_file.do_store(store_to_be_persisted)

    def check_consistency(self, crash_state_file, tx_index, crash_src_info):
        # execute the suffix
        if self.server_name == 'memcached':
            crash_state_output, returncode, crash_core_dump, timeout = \
                run_memcached(crash_state_file, self.op_file, self.mmap_size,
                              tx_index+1, -1, crash_state_file+'-output', self.tx_id)
        elif self.server_name == 'redis':
            crash_state_output, returncode, crash_core_dump, timeout = \
                run_redis(crash_state_file, self.op_file, self.mmap_size,
                              tx_index+1, -1, crash_state_file+'-output', self.tx_id)
        else:
            crash_state_output, returncode, crash_core_dump, timeout = \
                       self.execute_from_crash_state(crash_state_file, tx_index)

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

        # get 2 oracle: full and skip
        oracle_full, oracle_skip = self.get_oracles(tx_index)

        # compare the output with 2 oracles
        inconsistent, mismatch_res = self.compare_to_oracles(crash_state_output,
                                                             oracle_full,
                                                             oracle_skip,
                                                             tx_index)

        # update result
        self.update_result(crash_state_output,
                           crash_src_info,
                           crash_core_dump,
                           inconsistent,
                           mismatch_res)
        # remove the pm file for saving disk space
        self.cleanup_pm_file(crash_state_file)

    def update_result(self,
                      crash_state_output,
                      crash_src_info,
                      crash_core_dump,
                      should_report,
                      mismatch_res):
        # update tested crash plan
        self.tested_crash_plans.append(crash_state_output)

        # if update reported crash plan: either crash or output diff
        if should_report == True:
            self.reported_crash_plans.append(crash_state_output)

            if crash_src_info not in self.reported_src_map:
                self.reported_src_map[crash_src_info] = []
            self.reported_src_map[crash_src_info].append(crash_state_output)

            if crash_core_dump == None:
                assert(mismatch_res != None)
                crash_core_dump = 'output diff:' + mismatch_res

            if crash_core_dump not in self.reported_core_dump_map:
                self.reported_core_dump_map[crash_core_dump] = []
            self.reported_core_dump_map[crash_core_dump] \
                                                     .append(crash_state_output)

            if crash_src_info not in self.reported_priority:
                self.reported_priority[crash_src_info] = dict()
            if crash_core_dump not in self.reported_priority[crash_src_info]:
                self.reported_priority[crash_src_info][crash_core_dump] = []
            self.reported_priority[crash_src_info][crash_core_dump] \
                                                     .append(crash_state_output)

    # remove the pm file for saving disk space
    def cleanup_pm_file(self, crash_state_file):
        if self.keep_pm_file == True:
            return

        cmd = ['rm',
               '-f',
               crash_state_file]
        os.system(' '.join(cmd))

    def execute_from_crash_state(self, crash_state_file, tx_index):
        start_tx_index = tx_index + 1
        skip_tx_index = -1
        crash_state_output = crash_state_file + '-output'
        cmd = ['./' + self.validate_exe,
               crash_state_file,
               self.mmap_size,
               self.pmdk_pool_layout,
               self.op_file,
               str(start_tx_index),
               str(skip_tx_index),
               crash_state_output]

        getLogger().debug('validate cmd: ' + ' '.join(cmd))
        proc = Popen(cmd, stdout=PIPE, stderr=PIPE)
        # TODO for memcached do not use PIPE
        # proc = Popen(cmd)
        # set the timeout for validation
        # TODO 10s for now
        timeout = False
        try:
            proc.communicate(timeout=10)
            # TODO for memcached use 30s
            # proc.communicate(timeout=30)
        except TimeoutExpired:
            proc.kill()
            proc.communicate()
            timeout = True

        crash_core_dump = None
        # get the core dump if the return code is not 0
        if timeout == False and proc.returncode != 0:
            crash_core_dump = self.get_crash_core_dump(proc.pid)
        return crash_state_output, proc.returncode, crash_core_dump, timeout

    def get_crash_core_dump(self, pid):
        binary = self.validate_exe.split('/')[-1]
        # enable the core dump
        # path: /tmp/binary-pid.core
        # need to add the following line into the /etc/sysctl.conf
        # kernel.core_pattern=/tmp/%e-%p.core
        # and then sudo sysctl -p
        # or call just sudo sysctl -w kernel.core_pattern=/tmp/%e-%p.core
        # also need to add the following into .zshrc
        # ulimit -c unlimited
        core_dump_file= '/tmp/' + binary + '-' + str(pid) + '.core'
        if not os.path.isfile(core_dump_file):
            return 'abnormal exit with no core dump'

        # get the dump core
        # gdb binary core --batch
        # TODO: it is hard coded for now
        cmd = ['gdb', self.validate_exe, core_dump_file, '--batch']
        proc = Popen(cmd, stdout=PIPE, stderr=PIPE)
        out, err = proc.communicate()
        core_dump = out.decode()
        core_dump = core_dump.split('\n')
        ret = core_dump[-3].split(' ')[-1] + ':' + core_dump[-2]

        # clean up the core_dump_file for disk space
        cmd = ['rm',
               '-f',
               core_dump_file]
        os.system(' '.join(cmd))
        return ret

    def get_oracles(self, tx_index):
        # get oracle-full: cp it from the tracing output and put into dict
        if 'full' not in self.oracles:
            oracle_output_path = self.output_dir + '/oracle-full'
            cmd = ['cp',
                   self.full_oracle_file,
                   oracle_output_path]
            os.system(' '.join(cmd))
            self.oracles['full'] = oracle_output_path
        oracle_full = self.oracles['full']

        # get oracle-skip: run it and put into the dict
        if tx_index not in self.oracles:
            oracle_output_path = self.output_dir + \
                                 '/oracle-skip-tx-' + \
                                 str(tx_index)
            oracle_pm_path = oracle_output_path + '-pm'
            start_tx_index = 0
            skip_tx_index = tx_index

            if self.server_name == 'memcached':
                run_memcached(oracle_pm_path, self.op_file, self.mmap_size,
                              start_tx_index, skip_tx_index, oracle_output_path, self.tx_id)
            elif self.server_name == 'redis':
                run_redis(oracle_pm_path, self.op_file, self.mmap_size,
                          start_tx_index, skip_tx_index, oracle_output_path, self.tx_id)
            else:
                cmd = ['./' + self.validate_exe,
                       oracle_pm_path,
                       self.mmap_size,
                       self.pmdk_pool_layout,
                       self.op_file,
                       str(start_tx_index),
                       str(skip_tx_index),
                       oracle_output_path]
                subprocess.call(cmd, stdout=PIPE, stderr=PIPE)
                # TODO for memcached do not use PIPE
                # subprocess.call(cmd)

            self.oracles[tx_index] = oracle_output_path

            # remove the pm file for saving disk space
            self.cleanup_pm_file(oracle_pm_path)
        oracle_skip = self.oracles[tx_index]

        return oracle_full, oracle_skip

    def compare_to_oracles(self,
                           crash_state_output,
                           oracle_full,
                           oracle_skip,
                           tx_index):
        # compare with oracle_full
        oracle_full_match, oracle_full_tx_mismatch \
                         = self.compare_to_oracle_from_index(crash_state_output,
                                                             oracle_full,
                                                             tx_index+1)
        # compare with oracle_skip
        oracle_skip_match, oracle_skip_tx_mismatch \
                         = self.compare_to_oracle_from_index(crash_state_output,
                                                             oracle_skip,
                                                             tx_index)
        # if both not match, we report it
        inconsistent = False
        if not oracle_full_match and not oracle_skip_match:
            inconsistent = True

        return inconsistent, \
               oracle_full_tx_mismatch + ' ' + oracle_skip_tx_mismatch

    # compare crash_state_output to oracle from oracle_index
    def compare_to_oracle_from_index(self,
                                     crash_state_output,
                                     oracle,
                                     oracle_index):
        # should be handled by the core dump
        # TODO: if the file not exists, it means it crashed, we return False
        # if not os.path.isfile(crash_state_output):
        #    return False
        crash_state_output_list = []
        with open(crash_state_output, errors='ignore') as f:
            for line in f:
                crash_state_output_list.append(line)

        # TODO: if the file not exists, it means it crashed, we return False
        if not os.path.isfile(oracle):
            return False, 'crash'
        oracle_list = []
        with open(oracle, errors='ignore') as f:
            for line in f:
                oracle_list.append(line)
        oracle_list = oracle_list[oracle_index:]

        #assert(len(crash_state_output_list) == len(oracle_list))
        # the length may be different because execution may crash due to
        # inconsistency
        if len(crash_state_output_list) != len(oracle_list):
            return False, 'crash'

        for i in range(len(crash_state_output_list)):
            if crash_state_output_list[i] != oracle_list[i]:
                return False, 'tx-'+str(oracle_index+i)

        return True, 'match'
