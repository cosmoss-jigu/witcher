#!/usr/bin/python3

import os
import sys
import signal
import time
from pymemcache.client.base import Client

pm_file = sys.argv[1]
pm_size = sys.argv[2]
pm_layout = sys.argv[3]
op_file = sys.argv[4]
op_index = int(sys.argv[5])
skip_index = int(sys.argv[6])
output_file = sys.argv[7]
#memory_layout = sys.argv[8]

MEMCACHED_DIR = os.environ['WITCHER_HOME'] + '/benchmark/memcached-pmem'
MAIN_DIR = MEMCACHED_DIR + '/main'
LIB_DIR = MEMCACHED_DIR + '/lib'

# TODO
cmd = 'killall -9 ' + MAIN_DIR + '/main.exe'
os.system(cmd)

# init server cmd
if os.path.isfile(pm_file):
    # recover cmd
    cmd = [MAIN_DIR + '/main.exe',
           '-m 0 -o',
           'pslab_file=' + pm_file + ',pslab_force,pslab_recover']
else:
    # no recover cmd
    cmd = [MAIN_DIR + '/main.exe',
           '-m 0 -o',
           'pslab_file=' + pm_file + ',pslab_size=' + pm_size + ',pslab_force']

pid = os.fork()

if pid:
    # for server warm up
    time.sleep(2)

    # memcached connection
    client = Client(('localhost', 11211))
    op_list = open(op_file).read().split("\n")[:-1]
    output_list = []
    index = op_index
    op_length = len(op_list)
    while index < op_length:
        if index == skip_index:
            index += 1
            continue

        op = op_list[index]
        strs = op.split(";")[:-1]
        op_type = strs[0]
        if op_type == 'i':
            key = strs[1]
            val = strs[2]
            ret = client.set(key, val)
            output_list.append(ret)
        elif op_type == 'd':
            key = strs[1]
            ret = client.delete(key)
            output_list.append(ret)
        elif op_type == 'g':
            key = strs[1]
            ret = client.get(key)
            output_list.append(ret)
        else:
            # never come to here
            assert(False)

        index += 1

    client.quit()

    # write output to the file
    with open(output_file, 'w') as f:
        for line in output_list:
            f.write('%s\n' % line)

    # TODO
    # os.kill(pid, signal.SIGSTOP)
    cmd = 'killall -9 ' + MAIN_DIR + '/main.exe'
    os.system(cmd)
else:
    # start the server
    os.system(' '.join(cmd))
