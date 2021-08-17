#!/usr/bin/python3

import argparse
import pickle
from engines.witcherparallelfull.witcherparallelfullcrashmanager import WitcherParallelFullCrashManager

def main():
    parser = argparse.ArgumentParser(description="Witcher Parallel Unit")

    parser.add_argument("-args", "--args-pickle-path",
                        required=True,
                        help="args pickle path")

    parser.add_argument("-tx", "--tx-id",
                        required=True,
                        help="withcer tx id")

    parser.add_argument("-start", "--start",
                        required=True,
                        help="crash range start")

    parser.add_argument("-end", "--end",
                        required=True,
                        help="crash range end")

    parser.add_argument("-budget", "--budget",
                        required=True,
                        help="crash budget")

    args = parser.parse_args()
    args_from_pickle = pickle.load(open(args.args_pickle_path, 'rb'))
    tx_id = int(args.tx_id)
    start = int(args.start)
    end = int(args.end)
    budget= int(args.budget)

    witcher_parallel_full_crash_manager = \
                            WitcherParallelFullCrashManager(args_from_pickle,
                                                            tx_id,
                                                            start,
                                                            end,
                                                            budget)
    witcher_parallel_full_crash_manager.run()

if __name__ == "__main__":
    main()
