#!/usr/bin/python3

import os
import sys
import subprocess
from subprocess import PIPE
import time
import redis

os.system('rm -f dump.rdb')

pm_file = sys.argv[1]
pm_size = sys.argv[2] + 'mb'
pm_layout = sys.argv[3]
op_file = sys.argv[4]
op_index = int(sys.argv[5])
skip_index = int(sys.argv[6])
output_file = sys.argv[7]
#memory_layout = sys.argv[8]

REDIS_DIR = os.environ['WITCHER_HOME'] + '/benchmark/redis-3.2-nvml'
MAIN_DIR = REDIS_DIR + '/main'
LIB_DIR = REDIS_DIR + '/lib'

# start the redis server
cmd = [MAIN_DIR + '/main.exe',
       LIB_DIR + '/redis.conf',
       'pmfile',
       pm_file,
       pm_size]
proc = subprocess.Popen(cmd, stdout=PIPE, stderr=PIPE)
time.sleep(2)

# redis connection
r = redis.Redis(host='localhost', port=6379)
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
        key = 'key:'+strs[1]
        val = 'val:'+strs[2]
        ret = r.set(key, val)
        output_list.append(ret)
    elif op_type == 'd':
        key = 'key:'+strs[1]
        ret = r.delete(key)
        output_list.append(ret)
    elif op_type == 'g':
        key = 'key:'+strs[1]
        ret = r.get(key)
        output_list.append(ret)
    elif op_type == 'INIT':
        ret = ''
        output_list.append(ret)
    else:
        # never come to here
        assert(False)
    index += 1

r.shutdown(save=False)

# wait the redis server shuting down
proc.communicate()

# write output to the file
with open(output_file, 'w') as f:
    for line in output_list:
        f.write('%s\n' % line)
