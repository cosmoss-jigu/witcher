from mem.memoryoperations import TXStart, Store
from logging import getLogger
import os

PRINT_DIR = 'res'
PRINT_FILE_TRACE = 'atmoic_write.pmtrace'
PRINT_FILE_PM_OBJ = 'pm_object'
PRINT_FILE_BELIEF = 'belief'
PRINT_FILE_CRITICAL = 'critical_read'
PRINT_FILE_CRASH_CANDIDATES = 'crash_candidates'
PRINT_FILE_CRASH_TESTED = 'crash_plans_tested'
PRINT_FILE_CRASH_REPORTED = 'crash_plans_reported'
PRINT_FILE_CRASH_REPORTED_SRC_MAP = 'crash_plans_reported_src_map'
PRINT_FILE_CRASH_REPORTED_CORE_DUMP_MAP = 'crash_plans_reported_core_dump_map'
PRINT_FILE_CRASH_REPORTED_PRIO_MAP = 'crash_plans_reported_priority_map'

class WitcherResPrinter():
    def __init__(self, output_dir):
        self.output_dir = output_dir + '/' + PRINT_DIR
        os.system('mkdir ' + self.output_dir)

        self.trace_store_count = -1
        self.pm_object_count = -1
        self.belief_count = -1
        self.critical_read_count = -1
        self.tested_crash_plan_count = -1
        self.reported_crash_plan_count = -1

    def print_trace(self, trace):
        tx_start = False
        store_count = 0
        f = open(self.output_dir+'/'+PRINT_FILE_TRACE, 'w')
        for op in trace.atomic_write_ops:
            f.write(str(op) + '\n')
            if isinstance(op, TXStart):
                tx_start =  True
                continue
            if tx_start and isinstance(op, Store):
                store_count = store_count + 1
        self.trace_store_count = store_count

    def print_belief_database(self, belief_database):
        self.print_pm_objects(belief_database)
        self.print_belief_set(belief_database)
        self.print_critical_read_set(belief_database)

    def print_pm_objects(self, belief_database):
        f = open(self.output_dir+'/'+PRINT_FILE_PM_OBJ, 'w')
        pm_objects = belief_database.pm_object_list.list
        for pm_object in pm_objects:
            f.write(str(pm_object) + '\n')
        self.pm_object_count = len(pm_objects)

    def print_belief_set(self, belief_database):
        f = open(self.output_dir+'/'+PRINT_FILE_BELIEF, 'w')
        belief_set = belief_database.belief_set
        for belief in belief_set:
            f.write(str(belief) + '\n')
        self.belief_count = len(belief_set)

    def print_critical_read_set(self, belief_database):
        f = open(self.output_dir+'/'+PRINT_FILE_CRITICAL, 'w')
        critical_read_set = belief_database.critical_read_set
        for critical_read in critical_read_set:
            f.write(str(critical_read) + '\n')
        self.critical_read_count = len(critical_read_set)

    def print_crash_candidates(self, crash_candidates):
        getLogger().debug("crash_candidates: " + \
                          str(crash_candidates.crash_candidates_per_tx))
        f = open(self.output_dir+'/'+PRINT_FILE_CRASH_CANDIDATES, 'w')
        crash_candidates_per_tx_list = crash_candidates.crash_candidates_per_tx
        tx_count = 0
        for crash_candidates_per_tx in crash_candidates_per_tx_list:
            f.write('tx\t' + str(tx_count) + '\n')
            tx_count = tx_count + 1
            for crash_candidate in crash_candidates_per_tx:
                assert(len(crash_candidate) == 2)
                f.write(str(crash_candidate[0].id) + '\t')
                f.write(str(crash_candidate[1].id) + '\n')

    def print_crash_plans(self, crash_validator):
        self.print_tested_crash_plans(crash_validator)
        self.print_reported_crash_plans(crash_validator)
        self.print_reported_src_map(crash_validator)
        self.print_reported_core_dump_map(crash_validator)
        self.print_reported_priority(crash_validator)

    def print_tested_crash_plans(self, crash_validator):
        f = open(self.output_dir+'/'+PRINT_FILE_CRASH_TESTED, 'w')
        tested_crash_plans = crash_validator.tested_crash_plans
        for tested_crash_plan in tested_crash_plans:
            f.write(str(tested_crash_plan) + '\n')
        self.tested_crash_plan_count = len(tested_crash_plans)

    def print_reported_crash_plans(self, crash_validator):
        f = open(self.output_dir+'/'+PRINT_FILE_CRASH_REPORTED, 'w')
        reported_crash_plans = crash_validator.reported_crash_plans
        for reported_crash_plan in reported_crash_plans:
            f.write(str(reported_crash_plan) + '\n')
        self.repoted_crash_plan_count = len(reported_crash_plans)

    def print_reported_src_map(self, crash_validator):
        f = open(self.output_dir+'/'+PRINT_FILE_CRASH_REPORTED_SRC_MAP, 'w')
        reported_src_map = crash_validator.reported_src_map
        for src_info in reported_src_map:
            crash_plans = reported_src_map[src_info]
            f.write(str(src_info) + ': ' + str(len(crash_plans)) + '\n')
            for crash_plan in crash_plans:
                f.write(str(crash_plan) + '\n')
            f.write('\n')

    def print_reported_core_dump_map(self, crash_validator):
        f = open(self.output_dir+'/'+PRINT_FILE_CRASH_REPORTED_CORE_DUMP_MAP, \
                 'w')
        reported_core_dump_map = crash_validator.reported_core_dump_map
        for core_dump in reported_core_dump_map:
            crash_plans = reported_core_dump_map[core_dump]
            f.write(str(core_dump) + ': ' + str(len(crash_plans)) + '\n')
            for crash_plan in crash_plans:
                f.write(str(crash_plan) + '\n')
            f.write('\n')

    def print_reported_priority(self, crash_validator):
        f = open(self.output_dir+'/'+PRINT_FILE_CRASH_REPORTED_PRIO_MAP, 'w')
        reported_priority = crash_validator.reported_priority
        for src_info in reported_priority:
            f.write(str(src_info) + '\n')
            total = 0;
            core_dump_map = reported_priority[src_info]
            for core_dump in core_dump_map:
                crash_plans = core_dump_map[core_dump]
                total = total + len(crash_plans)
                f.write('\t' + str(core_dump) + ': ' + \
                        str(len(crash_plans)) + '\n')
                for crash_plan in crash_plans:
                    f.write('\t\t' + str(crash_plan) + '\n')
            f.write('Total:' + str(total) + '\n\n')

    def print_summary(self):
        f = open(self.output_dir+'/summary', 'w')
        f.write('trace_store_count: ' + str(self.trace_store_count) + '\n')
        f.write('pm_object_count: ' + str(self.pm_object_count) + '\n')
        f.write('belief_count: ' + str(self.belief_count) + '\n')
        f.write('critical_read_count: ' + str(self.critical_read_count) + '\n')
        f.write('tested_crash_plan_count: ' + \
                str(self.tested_crash_plan_count) + '\n')
        f.write('reported_crash_plan_count: ' + \
                str(self.repoted_crash_plan_count) + '\n')
