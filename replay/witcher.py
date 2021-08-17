#!/usr/bin/python3

import argparse
import engines.replayengines as replayengines
from engines.witcher.witcherengine import WitcherEngine
import mem.witchertrace as witchertrace
import misc.log as log

def main():
    parser = argparse.ArgumentParser(description="Witcher Replay")

    engines_keys = list(replayengines.engines.keys())
    parser.add_argument("-r", "--default-engine",
                        help="set default replay engine default=WitcherEngine",
                        choices=engines_keys,
                        default=engines_keys[0])

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

    # a file containing crash candidates generated before
    parser.add_argument("-cc", "--crash-candidates",
                        required=False,
                        help="given crash candidates, optional for witcher")

    # a string like this: tx_id-fence_id-crash_op_id
    parser.add_argument("-ct", "--crash-target",
                        required=False,
                        help="given crash target, optional for witcher")

    args = parser.parse_args()

    # setup the global logger
    log.setup_logger()

    # create the script context
    trace = witchertrace.WitcherTrace(args.tracefile)

    # get engine
    engine = replayengines.get_engine(args.default_engine)
    # set trace
    engine.set_trace(trace)
    if isinstance(engine, WitcherEngine):
        if (args.crash_target):
            engine.init_witcher_for_crash_target(args)
        else:
            if (args.crash_candidates):
                engine.init_witcher_from_crash_candidates(args)
            else:
                engine.init_witcher(args)
    # run the engine
    if (args.crash_target):
        engine.run_for_target()
    else:
        engine.run()

if __name__ == "__main__":
    main()
