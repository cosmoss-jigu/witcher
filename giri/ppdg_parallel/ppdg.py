#!/usr/bin/python3

import argparse
import sys,os
from ppdgparallel import PPDGParallel
sys.path.insert(1, os.environ['WITCHER_HOME']+'/replay/engines/witcherutils')
from WitcherParallelUtils import getThreadPoolStatus,startWitcherThreadPool,threadPoolQuit

def main():
    parser = argparse.ArgumentParser(description="PPDG Parallel")

    parser.add_argument("-opt", "--opt",
                        required=True,
                        help="LLVM opt path")

    parser.add_argument("-trace", "--trace-split",
                        required=True,
                        help="Trace split path")

    parser.add_argument("-giri", "--giri-lib",
                        required=True,
                        help="Giri lib path")

    parser.add_argument("-prefix", "--prefix",
                        required=True,
                        help="Processing file prefix")

    parser.add_argument("-pmaddr", "--pm-addr",
                        required=True,
                        help="PM start address")

    parser.add_argument("-pmsize", "--pm-size",
                        required=True,
                        help="PM size")

    parser.add_argument("-bc", "--bc-file",
                        required=True,
                        help="LLVM IR file")

    parser.add_argument("-o", "--output",
                        required=True,
                        help="Output path")

    parser.add_argument("-useTPL", "--useThreadPool",
                        required=True,
                        help="Initialize and use ThreadPool Library")

    args = parser.parse_args()
    args.useThreadPool = int(args.useThreadPool)
    if(args.useThreadPool):
        startWitcherThreadPool();
        poolStatus = getThreadPoolStatus()
        while poolStatus is "UNINITIALIZED":
            poolStatus = getThreadPoolStatus()
    ppdg_parallel = PPDGParallel(args)
    ppdg_parallel.run()
    if(args.useThreadPool):
        threadPoolQuit()

if __name__ == "__main__":
    main()
