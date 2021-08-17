#!/usr/bin/python3

import argparse
import pickle
import sys
from mem.memoryoperations import Store
from engines.witcher.witchercrashmanager import CrashCandidates
from engines.witcherparallel.witcherparallelserver import SocketClient
from engines.witcherparallel.witcherparallelserver import SocketCmdRunValidate
from engines.witcherparallel.witcherparallelserver import SocketCmdCrashCandidatesRes

# get crash candidates for a witcher tx
# generate validate command for each crash candidate
class CrashCandidatesParallel(CrashCandidates):
    def __init__(self, args_pickle, trace, belief_database, tx_id):
        self.args_pickle = args_pickle
        self.args = pickle.load(open(args_pickle, 'rb'))
        self.trace = trace
        self.belief_database = belief_database
        self.tx_id = tx_id

        # init the candidates
        self.get_crash_candidates_from_tx_id()

    def get_crash_candidates_from_tx_id(self):
        tx_range = self.trace.atomic_write_ops_tx_ranges[self.tx_id]
        # all ops in this tx
        ops = self.trace.atomic_write_ops[tx_range[0]:tx_range[1]+1]
        # all stores in this tx
        stores = list(filter(lambda op: isinstance(op, Store), ops))
        # init candidates of this tx
        self.crash_candidates = self.get_crash_candidates_from_tx(stores)

    def get_parallel_commands(self):
        cmds = []
        for entry in self.crash_candidates:
            cmd = ''
            pre_str_id = entry[0].id
            suc_str_id = entry[1].id
            cmd = self.args.witcher_parallel_path + \
                    '/witcher_parallel_validate.py'+ \
                    ' -args ' + self.args_pickle + \
                    ' -pre ' + str(pre_str_id) + \
                    ' -suc ' + str(suc_str_id) + \
                    ' -tx ' + str(self.tx_id)
            cmds.append(cmd)
        return cmds


def main():
    parser = argparse.ArgumentParser(description="Witcher Replay")
    parser.add_argument("-args", "--args",
                        required=True,
                        help="pickled args")
    parser.add_argument("-t", "--trace",
                        required=True,
                        help="pickled trace")
    parser.add_argument("-b", "--belief",
                        required=True,
                        help="pickled belief")
    parser.add_argument("-tx", "--tx-id",
                        required=True,
                        help="op(tx) id")
    args = parser.parse_args()

    trace = pickle.load(open(args.trace, 'rb'))
    belief_database = pickle.load(open(args.belief, 'rb'))
    tx_id = int(args.tx_id)

    # get crash candidates for a witcher tx
    crash_candidates = \
               CrashCandidatesParallel(args.args, trace, belief_database, tx_id)
    # initialize a socket client
    socket_client = SocketClient()
    # generate validate command for each crash candidate
    for cmd in crash_candidates.get_parallel_commands():
        # send the validate command to the server
        socket_client.send_socket_cmd(SocketCmdRunValidate(cmd))
    # send the crash candidate result command to the server
    socket_client.send_socket_cmd(SocketCmdCrashCandidatesRes(tx_id, \
                                             crash_candidates.crash_candidates))

if __name__ == "__main__":
    main()
