#!/usr/bin/python3

import os
import matplotlib.pyplot as plt

tasks = {
    "Level_Hashing" : os.getenv('WITCHER_HOME') + '/benchmark/Level_Hashing/random/2000',
    "FAST_FAIR" : os.getenv('WITCHER_HOME') + '/benchmark/FAST_FAIR/random/2000',
    "CCEH" : os.getenv('WITCHER_HOME') + '/benchmark/CCEH/random/2000',
}

cwd = os.getcwd()
print(cwd)

def draw_figure(name, path):

    def get_y(file_name):
        l = []
        f = open(file_name, 'r')
        for line in f.readlines()[1:]:
            res = int(line.split(",")[2])
            l.append(res)
        f.close()
        return l

    os.chdir(path)
    os.system('make replay-output-p-full3')

    yat = get_y("full.csv")
    witcher = get_y("witcher.csv")
    plt.plot(range(2000), witcher, 'r-', label='Witcher')
    plt.plot(range(2000), yat, 'b--', label='Yat')
    plt.legend()
    plt.yscale('log')
    plt.xticks([0, 500, 1000, 1500, 2000])
    plt.yticks([10e0, 10e6, 10e12])
    axes = plt.gca()
    axes.set_xlim([0,2000])
    axes.set_ylim([0,10e14])

    os.chdir(cwd)
    plt.savefig(name + '.png')
    plt.clf()

def main():
    for name, path in tasks.items():
        draw_figure(name, path)

if __name__ == "__main__":
    main()
