import redis
from subprocess import Popen, PIPE, TimeoutExpired
import os
import time


REDIS_DIR = os.environ['WITCHER_HOME'] + '/benchmark/redis-3.2-nvml'
MAIN_DIR = REDIS_DIR + '/main'
LIB_DIR = REDIS_DIR + '/lib'
MAIN_EXE = MAIN_DIR + '/main.exe'

PORT_BASE = 50000

def run_redis(pm_file, op_file, pm_size, op_index, skip_index, output_file, tx_id):
    pm_size += 'mb'
    # start server
    cmd = [MAIN_DIR + '/main.exe',
           LIB_DIR + '/redis.conf',
           'pmfile',
           pm_file,
           pm_size,
           '--port ' + str(PORT_BASE+tx_id)]

    proc = Popen(' '.join(cmd), shell=True, stdout=PIPE, stderr=PIPE)
    #proc = Popen(' '.join(cmd), shell=True)

    time.sleep(1)

    # redis connection
    r = redis.Redis(host='localhost', port=PORT_BASE+tx_id)
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

    #r.shutdown(save=True)
    r.shutdown(save=False)

    # write output to the file
    with open(output_file, 'w') as f:
        for line in output_list:
            f.write('%s\n' % line)

    #proc.kill()
    proc.communicate()

    return output_file, 0, None, False

#pm_file = 'fuck.img'
#op_file = '/home/fuxinwei/osdi21/witcher/benchmark/redis-3.2-nvml/test/basic/op_file.txt'
#pm_size = str(8)
#op_index = 0
#skip_index = -1
#output_file = 'output'
#tx_id = 0
#run_redis(pm_file, op_file, pm_size, op_index, skip_index, output_file, tx_id)
