import os
import pickle
import socket
from threading import Thread, Lock
from concurrent.futures import ProcessPoolExecutor

SERVER_HOST = '127.0.0.1'
SERVER_PORT = 50000

# Base for socket command
class SocketCmd:
    def __init__(self):
        pass

# Socket command for run validation
class SocketCmdRunValidate(SocketCmd):
    def __init__(self, cmd):
        self.cmd = cmd

# Socket command for receiving crash candidate result
class SocketCmdCrashCandidatesRes(SocketCmd):
    def __init__(self, tx_id, crash_candidates):
        self.tx_id = tx_id
        self.crash_candidates = crash_candidates

# Socket command for receiving validating result
class SocketCmdCrashValidateRes(SocketCmd):
    def __init__(self,
                 tested_crash_plans,
                 reported_crash_plans,
                 reported_src_map,
                 reported_core_dump_map,
                 reported_priority):
        self.tested_crash_plans  = tested_crash_plans
        self.reported_crash_plans = reported_crash_plans
        self.reported_src_map = reported_src_map
        self.reported_core_dump_map = reported_core_dump_map
        self.reported_priority = reported_priority

# A socket client to send socket command
class SocketClient:
    def __init__(self):
        pass

    def send_socket_cmd(self, socket_cmd):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((SERVER_HOST, SERVER_PORT))
            s.sendall(pickle.dumps(socket_cmd))

# A thread to initialize the server socket
class SocketInitThread(Thread):
    def __init__(self, server):
        Thread.__init__(self)
        self.server = server

    def run(self):
        s = self.server.socket
        s.bind((SERVER_HOST, SERVER_PORT))
        s.listen()
        while True:
            try:
                connection, addr = s.accept()
            except socket.error as msg:
                break
            socket_handler_thread = SocketHandlerThread(connection, self.server)
            socket_handler_thread.start()
        s.close()

# A thread to handle socket connection
class SocketHandlerThread(Thread):
    def __init__(self, connection, server):
        Thread.__init__(self)
        self.connection = connection
        self.server = server

    def run(self):
        socket_cmd_pickle = b''
        while True:
            data = self.connection.recv(1024)
            if not data:
                break
            socket_cmd_pickle = socket_cmd_pickle + data
        self.connection.close()
        self.server.handle_socket_cmd(pickle.loads(socket_cmd_pickle))

# A socket server with a pool executor
class WitcherParallelServer:
    def __init__(self, res_printer, num_of_txs):
        # initialize lock and counters
        self.init_thread_safe(num_of_txs)
        # initialize results for printer
        self.init_res(res_printer)
        # initialize server socket
        self.init_socket()
        # initialize pool executor
        self.init_pool_executor()

    # initialize lock and counters
    def init_thread_safe(self, num_of_txs):
        # thread lock for socket handler thread
        self.lock = Lock()
        # number of total witcher txs
        self.num_of_txs = num_of_txs
        # counter for crash candidate completed
        self.num_of_txs_done_crash_candidates = 0
        # flag for all crash candidate completed
        self.done_crash_candidates = False
        # counter for validate
        self.num_of_validate = 0
        # counter for validate completed
        self.num_of_validate_done = 0

    # initialize results for printer
    def init_res(self, res_printer):
        self.res_printer = res_printer
        self.crash_candidates = []
        self.tested_crash_plans  = []
        self.reported_crash_plans = []
        self.reported_src_map = []
        self.reported_core_dump_map = []
        self.reported_priority = []

    # initialize server socket
    def init_socket(self):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        socket_init_thread = SocketInitThread(self)
        socket_init_thread.start()

    # initialize pool executor
    def init_pool_executor(self):
        self.pool_executor = ProcessPoolExecutor(max_workers=os.cpu_count())

    # handle socket command
    def handle_socket_cmd(self, socket_cmd):
        # run validate
        if isinstance(socket_cmd, SocketCmdRunValidate):
            self.submit_to_executor(socket_cmd.cmd)

            # update validate counter
            with self.lock:
                self.num_of_validate += 1
            return

        # receive result of crash candidate
        if isinstance(socket_cmd, SocketCmdCrashCandidatesRes):
            # update res (thread-safe)
            self.crash_candidates.append([socket_cmd.tx_id, \
                                          socket_cmd.crash_candidates])

            # update crash candidate done counter
            # update all crash candidate done flag
            with self.lock:
                self.num_of_txs_done_crash_candidates += 1
                assert(self.num_of_txs_done_crash_candidates <= self.num_of_txs)
                if self.num_of_txs_done_crash_candidates == self.num_of_txs:
                    assert(self.done_crash_candidates == False)
                    self.done_crash_candidates = True
            return

        if isinstance(socket_cmd, SocketCmdCrashValidateRes):
            # update res (thread-safe)
            c = socket_cmd
            self.tested_crash_plans.append(c.tested_crash_plans)
            self.reported_crash_plans.append(c.reported_crash_plans)
            self.reported_src_map.append(c.reported_src_map)
            self.reported_core_dump_map.append(c.reported_core_dump_map)
            self.reported_priority.append(c.reported_priority)

            # update validate done counter
            # check wither to do the epilogue
            with self.lock:
                self.num_of_validate_done += 1
                assert(self.num_of_validate_done <= self.num_of_validate)
                if self.done_crash_candidates == True and \
                        self.num_of_validate_done == self.num_of_validate:
                    self.epilogue()
            return

        # never come to here
        assert(False)

    # submit a script to the executor
    def submit_to_executor(self, cmd):
        self.pool_executor.submit(os.system, cmd)

    def epilogue(self):
        # shut down the server
        self.socket.shutdown(socket.SHUT_RDWR)
        # print results
        self.res_printer.print_crash_candidates(self.crash_candidates)
        self.res_printer.print_tested_crash_plans(self.tested_crash_plans)
        self.res_printer.print_reported_crash_plans(self.reported_crash_plans)
        self.res_printer.print_reported_src_map(self.reported_src_map)
        self.res_printer.print_reported_core_dump_map(self.reported_core_dump_map)
        self.res_printer.print_reported_priority(self.reported_priority)
        self.res_printer.print_summary()
