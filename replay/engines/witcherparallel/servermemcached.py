from pymemcache.client.base import Client
from subprocess import Popen, PIPE, TimeoutExpired
import os
import time

MEMCACHED_DIR = os.environ['WITCHER_HOME'] + '/benchmark/memcached-pmem'
MAIN_DIR = MEMCACHED_DIR + '/main'
MAIN_EXE = MAIN_DIR + '/main.exe'

PORT_BASE = 50000

def run_memcached(pm_file, op_file, pm_size, op_index, skip_index, output_file, tx_id):
    # start server
    if os.path.isfile(pm_file):
        # recover cmd
        cmd = [MAIN_DIR + '/main.exe',
               '-p ' + str(PORT_BASE+tx_id),
               '-m 0 -o',
               'pslab_file=' + pm_file + ',pslab_force,pslab_recover']
    else:
        # no recover cmd
        cmd = [MAIN_DIR + '/main.exe',
               '-p ' + str(PORT_BASE+tx_id),
               '-m 0 -o',
               'pslab_file=' + pm_file + ',pslab_size=' + pm_size + ',pslab_force']

    proc = Popen(' '.join(cmd), shell=True)

    time.sleep(1)

    ## start client
    try:
        client = Client(('localhost', PORT_BASE+tx_id))
    except:
        client = None

    output_list = []

    if client != None:
        op_list = open(op_file).read().split("\n")[:-1]
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
                try:
                    ret = client.set(key, val)
                except:
                    break
                output_list.append(ret)
            elif op_type == 'd':
                key = strs[1]
                try:
                    ret = client.delete(key)
                except:
                    break
                output_list.append(ret)
            elif op_type == 'g':
                key = strs[1]
                try:
                    ret = client.get(key)
                except:
                    break
                output_list.append(ret)
            else:
                # never come to here
                assert(False)

            index += 1

        client.close()

    # write output to the file
    with open(output_file, 'w') as f:
        for line in output_list:
            f.write('%s\n' % line)

    proc.kill()
    proc.communicate()

    return output_file, 0, None, False

#pm_file = 'fuck.img.fuck'
#op_file = '/home/fuxinwei/osdi21/witcher/benchmark/memcached-pmem/test/basic/op_file.txt'
#pm_size = str(8)
#op_index = 0
#skip_index = -1
#output_file = 'output'
#tx_id = 0
#run_memcached(pm_file, op_file, pm_size, op_index, skip_index, output_file, tx_id)
