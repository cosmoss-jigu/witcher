#!/usr/bin/python3

import argparse
import pickle
from engines.witcherparallel.witcherparallelresanalyzer import WitcherParallelResAnalyzer

def main():
    parser = argparse.ArgumentParser(description="Witcher Parallel Unit")

    parser.add_argument("-input", "--input-path",
                        required=True,
                        help="input_path to get pickled files")

    parser.add_argument("-opfile", "--op-file",
                        required=True,
                        help="op file to get op types")

    parser.add_argument("-bb", "--trace-split-bb",
                        required=True,
                        help="path to get BB list for each TX")

    args = parser.parse_args()
    res_analyzer = WitcherParallelResAnalyzer(args.input_path, \
                                              args.op_file, \
                                              args.trace_split_bb)
    res_analyzer.run()

if __name__ == "__main__":
    main()
