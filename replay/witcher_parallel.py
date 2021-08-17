#!/usr/bin/python3

import argparse
import misc.log as log
from engines.witcherparallel.witcherparallelengine import WitcherParallelEngine
from engines.witcherutils.WitcherParallelUtils import getThreadPoolStatus,startWitcherThreadPool,threadPoolQuit

def main():
    parser = argparse.ArgumentParser(description="Witcher Parallel")

    parser.add_argument("-t", "--tracefile",
                        required=True,
                        help="the PM trace file to process")

    parser.add_argument("-p", "--ppdg",
                        required=True,
                        help="the ppdg file to process, required for witcher")

    parser.add_argument("-v", "--validate-exe",
                        required=True,
                        help="the validate exe, required for witcher")

    parser.add_argument("-pmfile", "--pmdk-mmap-file",
                        required=True,
                        help="the mmap file name, required for witcher")

    parser.add_argument("-pmaddr", "--pmdk-mmap-base-addr",
                        required=True,
                        help="the mmap file base address, required for witcher")

    parser.add_argument("-pmsize", "--pmdk-mmap-size",
                        required=True,
                        help="the mmap file size, required for witcher")

    parser.add_argument("-pmlayout", "--pmdk-create-layout",
                        required=True,
                        help="the pmdk layout, required for witcher")

    parser.add_argument("-opfile", "--validate-op-file",
                        required=True,
                        help="the op file for validating, required for witcher")

    parser.add_argument("-oracle", "--full-oracle-file",
                        required=True,
                        help="full tracing execution out, required for witcher")

    parser.add_argument("-o", "--output-dir",
                        required=True,
                        help="output directory path, required for witcher")

    parser.add_argument("-crash", "--crash",
                        required=True)

    parser.add_argument("-w", "--witcher-parallel-path",
                        required=True,
                        help="witcher replay code path")

    parser.add_argument("-useTPL", "--useThreadPool",
                        required=True,
                        help="initialize and use threadPool library")

    parser.add_argument("-server", "--server-name",
                        required=True,
                        help="na, redis, memcached")

    args = parser.parse_args()
    args.useThreadPool = int(args.useThreadPool)
    if args.useThreadPool:
        startWitcherThreadPool();
        poolStatus = getThreadPoolStatus()
        while poolStatus is "UNINITIALIZED":
            poolStatus = getThreadPoolStatus()
    witcher_parallel_engine = WitcherParallelEngine(args)
    witcher_parallel_engine.run()
    if args.useThreadPool:
        threadPoolQuit()

if __name__ == "__main__":
    main()
