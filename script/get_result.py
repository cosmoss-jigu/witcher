#!/usr/bin/python3

import os
import csv
import datetime
from tasks import tasks
from data import pmdk_set, filter_set

perf_res = {"libpmemobj" : [set(), set(), set(), set()]}
stats_res = {"libpmemobj" : ["-"] * 5}
overhead_res = {"libpmemobj" : ["-"] * 2}

def parse_perf_res(app_name, file_name, pos):
    if app_name not in perf_res:
        perf_res[app_name] = [set(), set(), set(), set()]
    f = open(file_name, 'r')
    for line in f.readlines():
        if line == "\n":
            break
        res = line.replace("KEY: ", "")
        res = res.replace("\n", "")
        if res in filter_set:
            continue
        elif res not in pmdk_set:
            perf_res[app_name][pos].add(res)
        else:
            perf_res["libpmemobj"][pos].add(res)
    f.close();

def get_perf_res(name ,path):
    path = path + '/tc'
    parse_perf_res(name, path+'/unfluhsed_stores', 0)
    parse_perf_res(name, path+'/extra_flushes', 1)
    parse_perf_res(name, path+'/extra_fences', 2)
    parse_perf_res(name, path+'/extra_logs', 3)

def get_stats(app_name, path):
    if app_name not in stats_res:
        stats_res[app_name] = []

    file_name = path + "/replay-output-p/res/summary"
    f = open(file_name, 'r')
    for line in f.readlines()[2:]:
        res = line.split(":")[1]
        res = res.replace("\n", "")
        stats_res[app_name].append(res)
    f.close();

    file_name = path + "/replay-output-p/res/classify_res_summary"
    f = open(file_name, 'r')
    line = f.readlines()[1]
    res = line.split(":")[1]
    res = res.replace("\n", "")
    stats_res[app_name].append(res)
    f.close();

def get_overhead():

    def get_time(time_str):
        [_, h, m, s] = time_str.split(":")
        return datetime.timedelta(hours=int(h), minutes=int(m), seconds=int(float(s)))

    file_name = "overhead.txt"
    f = open(file_name, 'r')
    for res in f.read().split("\n\n"):
        if res == "":
            continue
        l = res.split("\n")
        if len(l) == 5:
            l.pop(4)
        [app_name, ppdg, belief, validate] = l
        infer_time = get_time(ppdg) + get_time(belief)
        val_time = get_time(validate)

        if app_name not in overhead_res:
            overhead_res[app_name] = []
        overhead_res[app_name].append(infer_time)
        overhead_res[app_name].append(val_time)
    f.close();

def print_res():
    with open("res.csv", 'w') as csvfile:
        csvwriter = csv.writer(csvfile)
        row = ['Name','P-U','P-EFL','P-EFE','P-EF',
               '# ordering conditions', '# atomicity conditions', 'Inference execution time',
               '# crash NVM images', '# image with output mismatch', '# cluster', 'Checking execution time']
        csvwriter.writerow(row)
        for name in perf_res.keys():
            row = [name]
            for x in perf_res[name]:
               row.append(len(x))
            row.append(stats_res[name][0])
            row.append(stats_res[name][1])
            row.append(overhead_res[name][0])
            row.append(stats_res[name][2])
            row.append(stats_res[name][3])
            row.append(stats_res[name][4])
            row.append(overhead_res[name][1])
            csvwriter.writerow(row)

def main():
    for name, path in tasks.items():
        get_perf_res(name, path)
        get_stats(name, path)
    get_overhead()
    print_res()

if __name__ == "__main__":
    main()
