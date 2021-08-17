#!/usr/bin/python3

import argparse
import misc.log as log
from engines.witchertx.witchertxengine import WitcherTxEngine

def main():
    parser = argparse.ArgumentParser(description="Witcher Replay")

    parser.add_argument("-t", "--tracefile",
                        required=True,
                        help="the PM trace file to process")

    parser.add_argument("-pmdkop", "--pmdk-op-tracefile",
                        required=False,
                        help="the PMDK op trace file, required for witcher")

    parser.add_argument("-pmdkval", "--pmdk-val-tracefile",
                        required=False,
                        help="the PMDK val trace file, required for witcher")

    parser.add_argument("-p", "--ppdg",
                        required=False,
                        help="the ppdg file to process, required for witcher")

    parser.add_argument("-v", "--validate-exe",
                        required=False,
                        help="the validate exe, required for witcher")

    parser.add_argument("-pmfile", "--pmdk-mmap-file",
                        required=False,
                        help="the mmap file name, required for witcher")

    parser.add_argument("-pmaddr", "--pmdk-mmap-base-addr",
                        required=False,
                        help="the mmap file base address, required for witcher")

    parser.add_argument("-pmsize", "--pmdk-mmap-size",
                        required=False,
                        help="the mmap file size, required for witcher")

    parser.add_argument("-pmlayout", "--pmdk-create-layout",
                        required=False,
                        help="the pmdk layout, required for witcher")

    parser.add_argument("-opfile", "--validate-op-file",
                        required=False,
                        help="the op file for validating, required for witcher")

    parser.add_argument("-oracle", "--full-oracle-file",
                        required=False,
                        help="full tracing execution out, required for witcher")

    parser.add_argument("-o", "--output-dir",
                        required=False,
                        help="output directory path, required for witcher")

    args = parser.parse_args()
    witcher_tx_engine = WitcherTxEngine(args)
    witcher_tx_engine.run()

if __name__ == "__main__":
    main()
