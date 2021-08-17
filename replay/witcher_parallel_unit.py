#!/usr/bin/python3

import argparse
import pickle
from engines.witcherparallel.witcherparallelcrashmanager import WitcherParallelCrashManager

def main():
    parser = argparse.ArgumentParser(description="Witcher Parallel Unit")

    parser.add_argument("-args", "--args-pickle-path",
                        required=True,
                        help="args pickle path")

    parser.add_argument("-tx", "--tx-id",
                        required=True,
                        help="withcer tx id")

    args = parser.parse_args()
    args_from_pickle = pickle.load(open(args.args_pickle_path, 'rb'))
    tx_id = int(args.tx_id)

    witcher_parallel_crash_manager = \
                            WitcherParallelCrashManager(args_from_pickle, tx_id)
    witcher_parallel_crash_manager.run()

if __name__ == "__main__":
    main()
