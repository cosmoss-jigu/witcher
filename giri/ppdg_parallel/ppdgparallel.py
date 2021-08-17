import os,sys
from concurrent.futures import ProcessPoolExecutor, wait
sys.path.insert(1, os.environ['WITCHER_HOME']+'/replay/engines/witcherutils')
from WitcherParallelUtils import getThreadPoolStatus,threadPoolDispatchTask,threadPoolWait

class PPDGParallel:
    def __init__(self, args):
        self.init_args(args)
        self.init_output()
        self.init_trace_list()
        self.init_command_list()
        self.init_pool_executor()

    def run(self):
        futures = []
        for command in self.command_list:
            if(self.useTPL):
                threadPoolDispatchTask(command)
            else:
                futures.append(self.pool_executor.submit(os.system, command))
        if(self.useTPL):
            threadPoolWait()
        else:
            wait(futures)
        self.merge_ppdgs()

    def init_args(self, args):
        self.opt = args.opt
        self.trace_split = args.trace_split
        self.giri_lib = args.giri_lib
        self.prefix = args.prefix
        self.pm_addr = args.pm_addr
        self.pm_size = args.pm_size
        self.bc_file = args.bc_file
        self.output = args.output
        self.useTPL = args.useThreadPool

    def init_output(self):
        os.system('rm -rf ' + self.output)
        os.system('mkdir ' + self.output)

    def init_trace_list(self):
        path = self.trace_split
        self.trace_list = \
          [f for f in os.listdir(path) if os.path.isfile(os.path.join(path, f))]
        # sort the trace_list by trace size, which is used for load balance
        # analyze larger trace first
        self.trace_list.sort( \
              key=lambda f:os.path.getsize(os.path.join(path, f)), reverse=True)

    def init_command_list(self):
        self.command_list =[]
        for trace in self.trace_list:
            # TODO kill the ppdg analyze after 1 h if it doesn't finish
            command = 'timeout -s SIGKILL 2h ' + \
                      self.opt + \
                      ' -load ' + self.giri_lib + '/libdgutility.so' + \
                      ' -load ' + self.giri_lib + '/libgiri.so' + \
                      ' -load ' + self.giri_lib + '/libwitcher.so' + \
		              ' -mergereturn -bbnum -lsnum' + \
		              ' -dwitcherparallelpdg' + \
                      ' -trace-file=' + self.prefix + '.trace.split/' + trace +\
                      ' -pdg-file=' + self.output + '/' + trace + '.pdg' + \
		              ' -dwitcherparallelppdg' + \
		              ' -pm-addr=' + self.pm_addr + \
		              ' -pm-size=' + self.pm_size + \
                      ' -ppdg-file=' + self.output + '/' + trace + '.ppdg' + \
		              ' -remove-bbnum -remove-lsnum' + \
		              ' -stats '+ self.bc_file + ' -o /dev/null'
		              #' -stats -debug '+ self.bc_file + ' -o /dev/null'
            self.command_list.append(command)

    def init_pool_executor(self):
        self.pool_executor = ProcessPoolExecutor(max_workers=(os.cpu_count()-1))

    def merge_ppdgs(self):
        os.system('rm -f ' + self.prefix + '.ppdg')
        ppdg_merge_file = open(self.prefix + '.ppdg', 'a')
        for trace in self.trace_list:
            ppdg_file_name = self.output + '/'+ trace + '.ppdg'
            # TODO we tolerate some missing ppdgs
            if not os.path.isfile(ppdg_file_name):
                continue
            ppdg_file = open(ppdg_file_name, "r")
            ppdg_data = ppdg_file.read()
            ppdg_file.close()
            ppdg_merge_file.write(ppdg_data)
        ppdg_merge_file.close()
