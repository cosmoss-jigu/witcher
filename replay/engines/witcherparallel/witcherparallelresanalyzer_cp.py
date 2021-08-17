import pickle
from engines.witcherparallel.witcherparallelengine import PICKLE_TRACE
from engines.witcherparallel.witcherparallelengine import PICKLE_RES
from mem.memoryoperations import Store, Fence

# Crash Point class for a crash plan
class ReportedCrashPoint:
    def __init__(self, file_name, tx_id, fence_op, store_op, output_diff):
        self.file_name = file_name
        self.tx_id = tx_id
        self.fence_op = fence_op
        self.store_op = store_op
        self.output_diff = output_diff

# Res class for a TX
class ResInOneTx:
    def __init__(self, tx_id, tx_type):
        self.tx_id = tx_id
        self.tx_type = tx_type
        self.reported_crash_points = []

    def add_crash_point(self, crash_point):
        self.reported_crash_points.append(crash_point)

class WitcherParallelResAnalyzer:
    def __init__(self, input_path, op_file, trace_split_bb_path):
        self.input_path = input_path
        self.op_file = op_file
        self.trace_split_bb_path = trace_split_bb_path

        self.init_trace()
        self.init_tx_type_list()
        self.init_res_tx_type_map()

    # main function
    def run(self):
        # do classification based on BB list
        self.classify_based_on_bb()
        # print result
        self.print_classify_res_details()
        self.print_classify_res_summary()

    # initialize trace
    def init_trace(self):
        trace_path = self.input_path + '/' + PICKLE_TRACE
        self.trace = pickle.load(open(trace_path, 'rb'))

    # initialize tx type list
    def init_tx_type_list(self):
        txs = open(self.op_file).read().split("\n")
        txs = txs[:-1]
        self.tx_type_list = [tx[0] for tx in txs]

    # initialize res_tx_type_map
    # self.res_tx_type_map[tx_type][tx_id] = ReportedCrashPoint
    def init_res_tx_type_map(self):
        self.res_tx_type_map = dict()

        res_path = self.input_path + '/' + PICKLE_RES
        reported_priority = pickle.load(open(res_path, 'rb'))

        for priority_tx in reported_priority:
            for src_info in priority_tx:
                core_dump_map = priority_tx[src_info]
                for core_dump in core_dump_map:
                    crash_plans = core_dump_map[core_dump]
                    for crash_plan in crash_plans:
                        self.init_res_tx_type_map_from_crash_plan(crash_plan, \
                                                                  core_dump)

    # helper function for init_res_tx_type_map
    def init_res_tx_type_map_from_crash_plan(self, crash_plan, output_diff):
        tx_id, fence_id, store_id = self.parse_crash_plan(crash_plan)

        fence_op = self.trace.atomic_write_ops[fence_id]
        assert(isinstance(fence_op, Fence))

        #store_op = self.trace.atomic_write_ops[store_id]
        #assert(isinstance(store_op, Store) or isinstance(store_op, Fence))
        store_op = fence_op

        tx_type = self.tx_type_list[tx_id]

        if tx_type not in self.res_tx_type_map:
            self.res_tx_type_map[tx_type] = dict()

        if tx_id not in self.res_tx_type_map[tx_type]:
            self.res_tx_type_map[tx_type][tx_id] = ResInOneTx(tx_id, tx_type)

        res_in_one_tx = self.res_tx_type_map[tx_type][tx_id]
        crash_point = ReportedCrashPoint(crash_plan, tx_id, fence_op, \
                                         store_op, output_diff)
        res_in_one_tx.add_crash_point(crash_point)

    # parse a crash_plan to get [tx_id, fence_id, store_id]
    # crash_plan format: path/pm.img-txid-fenceid-storeid-output
    def parse_crash_plan(self, crash_plan):
        start_index = crash_plan.find('pm.img')
        crash_plan = crash_plan[start_index+7:]

        end_index = crash_plan.find('output')
        crash_plan = crash_plan[:end_index-1]

        ret = crash_plan.split('-')
        ret = [int(it) for it in ret]
        return ret

    # do classification based on BB list
    # self.classify_res[tx_type][BB_key] = ResInOneTx
    def classify_based_on_bb(self):
        self.classify_res = dict()
        for tx_type in self.res_tx_type_map:
            for tx_id in self.res_tx_type_map[tx_type]:
                res_in_one_tx = self.res_tx_type_map[tx_type][tx_id]
                bb_key = self.get_bb_key(tx_id)

                if tx_type not in self.classify_res:
                    self.classify_res[tx_type] = dict()

                if bb_key not in self.classify_res[tx_type]:
                    self.classify_res[tx_type][bb_key] = []

                self.classify_res[tx_type][bb_key].append(res_in_one_tx)

    # get BB_key for one tx_id
    # BB_key: a string of sorted of bb_ids (no duplicated bb ids)
    def get_bb_key(self, tx_id):
        # get bb list from bb_file
        bb_file = self.trace_split_bb_path + '/' + str(tx_id)
        bbs = open(bb_file).read().split("\n")
        bbs = bbs[:-1]
        # convert each bb_id string to int
        # convert the list to a set (eliminate duplicated bb_ids)
        # convert the set to a sorted list
        bbs = sorted(set([int(it) for it in bbs]))
        # convert the sorted list to a string
        bb_key = ''
        for bb in bbs:
            bb_key += str(bb) + ','
        return bb_key

    # print details of classification result
    def print_classify_res_details(self):
        f = open(self.input_path+'/res/classify_res_details', 'w')
        for tx_type in self.classify_res:
            f.write('TX_TYPE: ' + tx_type + '\n')
            for bb_key in self.classify_res[tx_type]:
                f.write('\tBB_KEY: ' + bb_key + '\n')
                for res_in_one_tx in self.classify_res[tx_type][bb_key]:
                    assert(res_in_one_tx.tx_type == tx_type)
                    tx_id = res_in_one_tx.tx_id
                    f.write('\t\tTX_ID: ' + str(tx_id) + '\n')
                    for crash_point in res_in_one_tx.reported_crash_points:
                        assert(crash_point.tx_id == tx_id)
                        f.write('\t\t\tfilename: ' + str(crash_point.file_name))
                        f.write('\tsrc: ' + str(crash_point.store_op.src_info))
                        f.write('\ttoutput_diff: ' + str(crash_point.output_diff))
                        f.write('\n')
                f.write('\n')
            f.write('\n')

    # print the summary of classification result
    def print_classify_res_summary(self):
        f = open(self.input_path+'/res/classify_res_summary', 'w')
        total_crash_plan_per_tx_type = dict()
        total_crash_plan_per_bb_key = dict()
        total_crash_plan_per_tx_id = dict()
        total_crash_plans = 0
        total_tx_types = 0
        total_bb_keys = 0
        total_tx_ids = 0

        for tx_type in self.classify_res:
            total_tx_types += 1
            for bb_key in self.classify_res[tx_type]:
                total_bb_keys += 1
                for res_in_one_tx in self.classify_res[tx_type][bb_key]:
                    total_tx_ids += 1

                    tx_id = res_in_one_tx.tx_id
                    length = len(res_in_one_tx.reported_crash_points)

                    if tx_type not in total_crash_plan_per_tx_type:
                        total_crash_plan_per_tx_type[tx_type] = 0
                    total_crash_plan_per_tx_type[tx_type] += length

                    if bb_key not in total_crash_plan_per_bb_key:
                        total_crash_plan_per_bb_key[bb_key] = 0
                    total_crash_plan_per_bb_key[bb_key] += length

                    if bb_key not in total_crash_plan_per_tx_id:
                        total_crash_plan_per_tx_id[tx_id] = 0
                    total_crash_plan_per_tx_id[tx_id] += length

                    total_crash_plans += length

        f.write('Total TX Types: ' + str(total_tx_types) + '\n')
        f.write('Total BB Keys: ' + str(total_bb_keys) + '\n')
        f.write('Total TX Ids: ' + str(total_tx_ids) + '\n')
        f.write('Total Crash Plans: ' + str(total_crash_plans) + '\n')
        f.write('\n')
        for tx_type in self.classify_res:
            f.write('TX_TYPE: ' + tx_type)
            f.write(' Total BB Keys: ' + str(len(self.classify_res[tx_type])))
            f.write(' Total Crash Plans: ' + str(total_crash_plan_per_tx_type[tx_type]))
            f.write('\n')
            bb_key_index = 0
            for bb_key in self.classify_res[tx_type]:
                f.write('\tBB_KEY ' + str(bb_key_index) + ':')
                f.write(' Total TX Ids: ' + str(len(self.classify_res[tx_type][bb_key])))
                f.write(' Total Crash Plans: ' + str(total_crash_plan_per_bb_key[bb_key]))
                f.write('\n')

                for res_in_one_tx in self.classify_res[tx_type][bb_key]:
                    tx_id = res_in_one_tx.tx_id
                    f.write('\t\tTX_ID: ' + str(tx_id) + ':')
                    f.write(' Total Crash Plans: ' + str(total_crash_plan_per_tx_id[tx_id]))
                    f.write('\n')
                bb_key_index += 1
            f.write('\n')
