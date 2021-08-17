import os
from engines.witcher.witcherresprinter import WitcherResPrinter
from engines.witcher.witcherresprinter import PRINT_FILE_CRASH_CANDIDATES
from engines.witcher.witcherresprinter import PRINT_FILE_CRASH_TESTED
from engines.witcher.witcherresprinter import PRINT_FILE_CRASH_REPORTED
from engines.witcher.witcherresprinter import PRINT_FILE_CRASH_REPORTED_SRC_MAP
from engines.witcher.witcherresprinter import PRINT_FILE_CRASH_REPORTED_CORE_DUMP_MAP
from engines.witcher.witcherresprinter import PRINT_FILE_CRASH_REPORTED_PRIO_MAP

class WitcherParallelResPrinter(WitcherResPrinter):
    def __init__(self, output_dir):
        WitcherResPrinter.__init__(self, output_dir)

    # need to sort based tx id
    def print_crash_candidates(self, crash_candidates):
        crash_candidates.sort(key=lambda x: x[0])
        crash_candidates_per_tx_list = [x[1] for x in crash_candidates]
        f = open(self.output_dir+'/'+PRINT_FILE_CRASH_CANDIDATES, 'w')
        tx_count = 0
        for crash_candidates_per_tx in crash_candidates_per_tx_list:
            f.write('tx\t' + str(tx_count) + '\n')
            tx_count = tx_count + 1
            for crash_candidate in crash_candidates_per_tx:
                assert(len(crash_candidate) == 2)
                f.write(str(crash_candidate[0].id) + '\t')
                f.write(str(crash_candidate[1].id) + '\n')

    # need to filter out duplicates
    def print_tested_crash_plans(self, tested_crash_plans):
        crash_plan_set = set()
        f = open(self.output_dir+'/'+PRINT_FILE_CRASH_TESTED, 'w')
        for tested_crash_plan_list in tested_crash_plans:
            for tested_crash_plan in tested_crash_plan_list:
                crash_plan = os.path.basename(tested_crash_plan)
                if crash_plan in crash_plan_set:
                    continue
                crash_plan_set.add(crash_plan)
                f.write(str(crash_plan) + '\n')
        self.tested_crash_plan_count = len(crash_plan_set)

    # need to filter out duplicates
    def print_reported_crash_plans(self, reported_crash_plans):
        crash_plan_set = set()
        f = open(self.output_dir+'/'+PRINT_FILE_CRASH_REPORTED, 'w')
        for reported_crash_plan_list in reported_crash_plans:
            for reported_crash_plan in reported_crash_plan_list:
                crash_plan = os.path.basename(reported_crash_plan)
                if crash_plan in crash_plan_set:
                    continue
                crash_plan_set.add(crash_plan)
                f.write(str(crash_plan) + '\n')
        self.repoted_crash_plan_count = len(crash_plan_set)

    # need to reconstruct from a list
    def print_reported_src_map(self, reported_src_map):
        src_map = dict()
        for src_map_tx in reported_src_map:
            for src_info in src_map_tx:
                crash_plans = src_map_tx[src_info]
                for crash_plan in crash_plans:
                    crash_plan = os.path.basename(crash_plan)
                    if src_info not in src_map:
                        src_map[src_info] = set()
                    src_map[src_info].add(crash_plan)

        f = open(self.output_dir+'/'+PRINT_FILE_CRASH_REPORTED_SRC_MAP, 'w')
        for src_info in src_map:
            crash_plans = src_map[src_info]
            f.write(str(src_info) + ': ' + str(len(crash_plans)) + '\n')
            for crash_plan in crash_plans:
                f.write(str(crash_plan) + '\n')
            f.write('\n')

    # need to reconstruct from a list
    def print_reported_core_dump_map(self, reported_core_dump_map):
        core_dump_map = dict()
        for core_dump_map_tx in reported_core_dump_map:
            for core_dump in core_dump_map_tx:
                crash_plans = core_dump_map_tx[core_dump]
                for crash_plan in crash_plans:
                    crash_plan = os.path.basename(crash_plan)
                    if core_dump not in core_dump_map:
                        core_dump_map[core_dump] = set()
                    core_dump_map[core_dump].add(crash_plan)

        f = open(self.output_dir+'/'+PRINT_FILE_CRASH_REPORTED_CORE_DUMP_MAP, \
                 'w')
        for core_dump in core_dump_map:
            crash_plans = core_dump_map[core_dump]
            f.write(str(core_dump) + ': ' + str(len(crash_plans)) + '\n')
            for crash_plan in crash_plans:
                f.write(str(crash_plan) + '\n')
            f.write('\n')

    # need to reconstruct from a list
    def print_reported_priority(self, reported_priority):
        priority = dict()
        for priority_tx in reported_priority:
            for src_info in priority_tx:
                core_dump_map = priority_tx[src_info]
                for core_dump in core_dump_map:
                    crash_plans = core_dump_map[core_dump]
                    for crash_plan in crash_plans:
                        crash_plan = os.path.basename(crash_plan)
                        if src_info not in priority:
                            priority[src_info] = dict()
                        if core_dump not in priority[src_info]:
                            priority[src_info][core_dump] = set()
                        priority[src_info][core_dump].add(crash_plan)

        f = open(self.output_dir+'/'+PRINT_FILE_CRASH_REPORTED_PRIO_MAP, 'w')
        for src_info in priority:
            f.write(str(src_info) + '\n')
            total = 0;
            core_dump_map = priority[src_info]
            for core_dump in core_dump_map:
                crash_plans = core_dump_map[core_dump]
                total = total + len(crash_plans)
                f.write('\t' + str(core_dump) + ': ' + \
                        str(len(crash_plans)) + '\n')
                for crash_plan in crash_plans:
                    f.write('\t\t' + str(crash_plan) + '\n')
            f.write('Total:' + str(total) + '\n\n')
