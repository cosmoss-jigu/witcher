#!/usr/bin/python3

import os
import datetime
from tasks import tasks

f = open('overhead.txt', 'w')

def run(app_name, path):
    print(app_name + " starts...")

    f.write(app_name + '\n')

    os.chdir(path)
    os.system('make clean')
    os.system('make main.trace >/dev/null 2>/dev/null')
    os.system('make main.trace.split >/dev/null')
    os.system('make main.trace.split.bb >/dev/null')
    os.system('make main.pmtrace >/dev/null 2>/dev/null')

    t0 = datetime.datetime.now()
    os.system('make main.ppdg >/dev/null 2>/dev/null')
    t1 = datetime.datetime.now()
    f.write('ppdg: ' + str(t1-t0) + '\n')
    f.flush()

    os.system('make replay-output-p >/dev/null 2>/dev/null')
    f_res = open('overhead.txt', 'r')
    f.writelines(f_res.readlines())
    f.flush()
    f_res.close()

    os.system('make res_analysis >/dev/null')
    os.system('make tc >/dev/null')

    os.system('rm -f /tmp/main.exe* /tmp/opt-*')

    f.write('\n')
    f.flush()

    print(app_name + " ends.")

def main():
    for name, path in tasks.items():
        run(name, path)

if __name__ == "__main__":
    main()
