from engines.witcher.binaryfile import BinaryFile
from mem.memoryoperations import Store, Fence
from logging import getLogger
import os
import subprocess
from subprocess import Popen, PIPE, TimeoutExpired
from engines.witcherparallel.servermemcached import run_memcached
from engines.witcherparallel.serverredis import run_redis
from engines.witcher.witchercrashvalidator import WitcherCrashValidator

class WitcherFullCrashValidator(WitcherCrashValidator):

    # validate at tx_index fence_index wiht crash_plans
    def validate(self, tx_index, fence_index, crash_plans):
        # validate crash_plan one by one
        for crash_plan in crash_plans:
            # construct the crash pm first
            crash_state_file = self.construct_crash_state(tx_index,
                                                          fence_index,
                                                          crash_plan)
            # then check its consistency
            if len(crash_plan[1]) == 0:
                src_info = crash_plan[0].src_info
            else:
                src_info = crash_plan[1][0].src_info
            self.check_consistency(crash_state_file, tx_index, src_info)

    # copy the current pm into the output dir
    def generate_current_state(self, tx_index, fence_index, crash_plan):
        # flush the pm.img before copy
        self.cache.binary_file.flush()
        # file path: replay_pm_file-tx_index-store_id
        crash_state_file = self.replay_pm_file + '-' + \
                           str(tx_index) + '-' + \
                           str(fence_index) + '-' + \
                           str(crash_plan[0])

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
        if len(crash_plan[1]) == 0:
            return

        # init the binary file of the current pm
        binary_file = BinaryFile(crash_state_file, self.mmap_addr)
        # get all the stores should be persisted
        # (1) crash_plan itself
        # (2) all stores in the same cacheline happens before it
        stores_to_be_persisted = []
        for store in crash_plan[1]:
            stores_to_be_persisted += self.cache.get_stores_from_crash_plan(store)
        # persist them one by one
        for store_to_be_persisted in stores_to_be_persisted:
            binary_file.do_store(store_to_be_persisted)
