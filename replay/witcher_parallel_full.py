#!/usr/bin/python3

import argparse
import misc.log as log
from engines.witcherparallelfull.witcherparallelfullengine import WitcherParallelFullEngine

def main():
    parser = argparse.ArgumentParser(description="Witcher Parallel")

    parser.add_argument("-t", "--tracefile",
                        required=True,
                        help="the PM trace file to process")

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

    parser.add_argument("-w", "--witcher-parallel-path",
                        required=True,
                        help="witcher replay code path")

    parser.add_argument("-plan", "--plan",
                        required=True,
                        help="plan")

    args = parser.parse_args()
    witcher_parallel_full_engine = WitcherParallelFullEngine(args)
    witcher_parallel_full_engine.run()

if __name__ == "__main__":
    main()
