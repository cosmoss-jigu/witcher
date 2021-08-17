#!/usr/bin/python3

import argparse
import misc.log as log
from engines.witchermt.witchermtengine import WitcherMtEngine

def main():
    parser = argparse.ArgumentParser(description="Witcher Replay")

    parser.add_argument("-t", "--tracefile",
                        required=True,
                        help="the PM trace file to process")

    parser.add_argument("-psi", "---parallel-start-index",
                        required=True,
                        help="Parallel Start Index")

    parser.add_argument("-nt", "---num-threads",
                        required=True,
                        help="Number of threads")

    parser.add_argument("-pmdkop", "--pmdk-op-tracefile",
                        required=True,
                        help="the PMDK op trace file, required for witcher")

    parser.add_argument("-pmdkval", "--pmdk-val-tracefile",
                        required=True,
                        help="the PMDK val trace file, required for witcher")

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

    args = parser.parse_args()

    # setup the global logger
    log.setup_logger()

    witcher_mt_engine = WitcherMtEngine(args)
    witcher_mt_engine.run()

if __name__ == "__main__":
    main()
