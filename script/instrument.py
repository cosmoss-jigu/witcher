#!/usr/bin/python3

import os
from concurrent.futures import ProcessPoolExecutor, wait
from tasks import tasks

make_cmds = {
    "woart"          : "make include lib main",
    "wort"           : "make include lib main",
    "FAST_FAIR"      : "make include lib main",
    "Level_Hashing"  : "make include lib main",
    "CCEH"           : "make util src main",
    #
    "P-ART"          : "make include lib main",
    "P-BwTree"       : "make include lib main",
    "P-CLHT"         : "make include lib main",
    "P-CLHT-Aga"     : "make include lib main",
    "P-CLHT-Aga-TX"  : "make include lib main",
    "P-HOT"          : "make include main",
    "P-Masstree"     : "make include lib main",
    #
    'btree'          : "make include lib main",
    "ctree"          : "make include lib main",
    "rbtree"         : "make include lib main",
    "rbtree-aga"     : "make include lib main",
    "hashmap_tx"     : "make include lib main",
    "hashmap_atomic" : "make include lib main",
    #
    "mmecached-pmem" : "./init.sh; cd main; make",
    "redis-3.2-nvml" : "make include lib main",
}

def get_inst_cmd(app_name, path):
    path = path + "/../.."
    cmd = "cd " + path + ";"
    cmd += make_cmds[app_name]
    return cmd

def main():
    cmds = []
    for name, path in tasks.items():
        cmds.append(get_inst_cmd(name, path))
    futures = []
    pool_executor = ProcessPoolExecutor(max_workers=os.cpu_count()-1)
    for cmd in cmds:
        futures.append(pool_executor.submit(os.system, cmd))
    wait(futures)

if __name__ == "__main__":
    main()
