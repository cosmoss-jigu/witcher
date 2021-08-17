#!/usr/bin/python3

import argparse
import pickle
from engines.witcherparallel.witcherparallelcrashmanager import WitcherParallelCrashManager

def main():
    parser = argparse.ArgumentParser(description="Witcher Replay")
    parser.add_argument("-args", "--args",
                        required=True,
                        help="pickled args")
    parser.add_argument("-pre", "--pre-store-id",
                        required=True,
                        help="pre store id")
    parser.add_argument("-suc", "--suc-store-id",
                        required=True,
                        help="suc store id")
    parser.add_argument("-tx", "--tx-id",
                        required=True,
                        help="op(tx) id")
    args = parser.parse_args()

    pre_store_id = int(args.pre_store_id)
    suc_store_id = int(args.suc_store_id)
    tx_id = int(args.tx_id)
    args_from_pickle = pickle.load(open(args.args, 'rb'))
    witcher_parallel_crash_manager = WitcherParallelCrashManager( \
                                                               tx_id,
                                                               pre_store_id,
                                                               suc_store_id,
                                                               args_from_pickle)
    witcher_parallel_crash_manager.run()

if __name__ == "__main__":
    main()
